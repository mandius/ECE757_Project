# -*- coding: utf-8 -*-
# Copyright (c) 2015 Jason Power
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

""" This file creates a system with Ruby caches and executes different programs on two cores.
One program is victim code. Other is attach code.
System : Two private L1 caches, One shared L2 cache, One Memory
"""

# import the m5 (gem5) library created when gem5 is built
import m5
# import all of the SimObjects
from m5.objects import *
exe_dir = os.environ['EXE_DIR']
config_dir =os.environ['CONFIG_DIR']


# Needed for running C++ threads
m5.util.addToPath(config_dir)
from common.FileSystemConfig import config_filesystem

# You can import ruby_caches_MI_example to use the MI_example protocol instead
# of the MSI protocol
from MESI_Two_Level_caches import MESITwoLevelCache

# create the system we are going to simulate
system = System()

# Set the clock frequency of the system (and all of its children)
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '1GHz'
system.clk_domain.voltage_domain = VoltageDomain()

# Set up the system
system.mem_mode = 'timing'               # Use timing accesses
#system.mem_ranges = [AddrRange('512MB')] # Create an address range
system.mem_ranges = [AddrRange('8192MB')] # Create an address range

# Create a pair of simple CPUs
system.cpu = [TimingSimpleCPU() for i in range(2)]

# Create a DDR3 memory controller and connect it to the membus
system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]

# create the interrupt controller for the CPU and connect to the membus
for cpu in system.cpu:
    cpu.createInterruptController()

# Create the Ruby System
system.caches = MESITwoLevelCache()
system.caches.setup(system, system.cpu, [system.mem_ctrl])

# get ISA for the binary to run.
isa = str(m5.defines.buildEnv['TARGET_ISA']).lower()

# Run application and use the compiled ISA to find the binary
# grab the specific path to the binary
thispath = os.path.dirname(os.path.realpath(__file__))
#binary = os.path.join(thispath, '../../../', 'tests/test-progs/threads/bin/',
                  #    isa, 'linux/threads')
binary1 = os.path.join(exe_dir, 'attacker')
binary2 = os.path.join(exe_dir, 'victim')


# Create a process for a simple "multi-threaded" application
process1 = Process()
process2 = Process()
# Set the command
# cmd is a list which begins with the executable (like argv)
process1.cmd = [binary1]
process2.cmd = [binary2]
process1.pid = 100;
process2.pid = 101;
# Set the cpu to use the process as its workload and create thread contexts
#for cpu in system.cpu:
#    print("Loop start")
#    cpu.workload = process
#    cpu.createThreads()
#    print("Loop End")
system.cpu[0].workload=process1;
system.cpu[0].createThreads();
system.cpu[1].workload=process2;
system.cpu[1].createThreads();

system.workload = SEWorkload.init_compatible(binary1)

# Set up the pseudo file system for the threads function above
config_filesystem(system)

# set up the root SimObject and start the simulation
root = Root(full_system = False, system = system)
# instantiate all of the objects we've created above
m5.instantiate()

process1.map(0x00005000, 0x00005000,8192 , True)
process1.map(0x0A000000, 0xA000000,20971520 , True)
process2.map(0x00005000, 0x00005000, 8192, True)



print("Beginning simulation!")
exit_event = m5.simulate()
print('Exiting @ tick {} because {}'.format(
         m5.curTick(), exit_event.getCause())
     )