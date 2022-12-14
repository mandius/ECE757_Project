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
    uint64_t a, d;
    int ar, dr;
    __asm__ volatile ("rdtsc" : "=a" (ar), "=d" (dr) );
 
    a= ar;
    d= dr;
    a = (d << 32) | a;
    return a;
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



//int get_L1_cachhe_set(uint64_t *p) {
#define CACHE_SET_BYTES 512 * 64

void wait(int n) {
	for( int i=0; i< n; i++);
}


int main(int argc, char* argv[])
{
    volatile int* victim_address = (int*) 0x6000;
    int* sync_point_v2a = (int*) 0x5000;
    int* sync_point_a2v = (int*) 0x5004;
    volatile uint64_t start;
    int val;

    
    //Specify 5 bit access pattern
    int access_pattern = 0b10110;

    *sync_point_v2a = 0;
    printf("EXE_LOG:: Victim Address= %0x\n", victim_address);
    
   
    start=rdtsc();
    wait_on_rdtsc(start,10000);


    //barrier
    *sync_point_v2a=1;
    while(*sync_point_a2v==0) {;}


    printf("EXE_LOG:: Victim:: Past the sync_point\n");

    wait(100);
    //Access the victim address in a pattern defined by the access pattern
    for( int i=0; i<5  ; i++){

	while(*sync_point_a2v==0) {;}
	*sync_point_a2v=0;

	if((access_pattern>>i)& 0x1){
		printf("EXE_LOG:: ====================Accessing the Victim iter=%0d========================\n", i);
		 __asm__ volatile ("mfence");
		val = *victim_address;
		 __asm__ volatile ("mfence");
		printf("EXE_LOG:: ====================Accessing the Victim iter=%0d========================\n", i);
	}

	*sync_point_v2a=1;
    }


    return 0;
}

