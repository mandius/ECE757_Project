from __future__ import print_function
from __future__ import absolute_import

import math

from m5.defines import buildEnv
from m5.util import fatal, panic

from m5.objects import *

class MESITwoLevelCache(RubySystem):

    def __init__(self):
        if buildEnv['PROTOCOL'] != 'MESI_Two_Level':
            fatal("This system assumes MESI_Two_Level!")

        super(MESITwoLevelCache, self).__init__()

        self._numL2Caches = 8

    def setup(self, system, cpus, mem_ctrls):
        """Set up the Ruby cache subsystem. Note: This can't be done in the
           constructor because many of these items require a pointer to the
           ruby system (self). This causes infinite recursion in initialize()
           if we do this in the __init__.
        """
        # Ruby's global network.
        self.network = MyNetwork(self)

        # MESI_Two_Level example uses 5 virtual networks
        self.number_of_virtual_networks = 5
        self.network.number_of_virtual_networks = 5

        # There is a single global list of all of the controllers to make it
        # easier to connect everything to the global network. This can be
        # customized depending on the topology/network requirements.
        # L1 caches are private to a core, hence there are one L1 cache per CPU core.
        # The number of L2 caches are dependent to the architecture.
        self.controllers = \
            [L1Cache(system, self, cpu, self._numL2Caches) for cpu in cpus] + \
            [L2Cache(system, self, self._numL2Caches) for num in range(self._numL2Caches)] + \
            [DirController(self, system.mem_ranges, mem_ctrls)]
        
        # Create one sequencer per CPU and dma controller. 
        # Sequencers for other controllers can be here here.
        self.sequencers = [RubySequencer(version = i,
                                # I/D cache is combined and grab from ctrl
                                #icache = self.controllers[i].L1Icache,
                                dcache = self.controllers[i].L1Dcache,
                                clk_domain = self.controllers[i].clk_domain
                                ) for i in range(len(cpus))
                          ]

        for i,c in enumerate(self.controllers[:len(cpus)]):
            c.sequencer = self.sequencers[i]


        self.num_of_sequencers = len(self.sequencers)

        # Create the network and connect the controllers.
        # NOTE: This is quite different if using Garnet!
        self.network.connectControllers(self.controllers)
        self.network.setup_buffers()

        # Set up a proxy port for the system_port. Used for load binaries and
        # other functional-only things.
        self.sys_port_proxy = RubyPortProxy()
        system.system_port = self.sys_port_proxy.in_ports

        # Connect the cpu's cache, interrupt, and TLB ports to Ruby
        for i,cpu in enumerate(cpus):
            self.sequencers[i].connectCpuPorts(cpu)            
            #cpu.icache_port = self.sequencers[i].in_port
            #cpu.dcache_port = self.sequencers[i].in_port
            #isa = buildEnv['TARGET_ISA']
            #if isa == 'x86':
            #    cpu.interrupts[0].pio = self.sequencers[i].out_port
            #    cpu.interrupts[0].int_out_port = self.sequencers[i].in_port
            #    cpu.interrupts[0].int_in_port = self.sequencers[i].out_port
            #if isa == 'x86' or isa == 'arm':
            #    cpu.itb.walker.port = self.sequencers[i].in_port
            #    cpu.dtb.walker.port = self.sequencers[i].in_port


class L1Cache(L1Cache_Controller):

    _version = 0
    @classmethod
    def versionCount(cls):
        cls._version += 1 # Use count for this particular type
        return cls._version - 1

    def __init__(self, system, ruby_system, cpu, num_l2Caches):
        """Creating L1 cache controller. Consist of both instruction
           and data cache. The size of data cache is 512KB and
           8-way set associative. The instruction cache is 32KB,
           2-way set associative.
        """
        super(L1Cache, self).__init__()
        
        self.version = self.versionCount()
        block_size_bits = int(math.log(system.cache_line_size, 2))
        l1i_size = '32kB'
        l1i_assoc = '2'
        l1d_size = '512kB'
        l1d_assoc = '8'
        # This is the cache memory object that stores the cache data and tags
        self.L1Icache = RubyCache(size = l1i_size,
                                assoc = l1i_assoc,
                                start_index_bit = block_size_bits ,
                                is_icache = True)
        self.L1Dcache = RubyCache(size = l1d_size,
                            assoc = l1d_assoc,
                            start_index_bit = block_size_bits,
                            is_icache = False)
        self.l2_select_num_bits = int(math.log(num_l2Caches , 2))
        self.clk_domain = cpu.clk_domain
        self.prefetcher = RubyPrefetcher()
        self.send_evictions = self.sendEvicts(cpu)
        self.transitions_per_cycle = 4
        self.enable_prefetch = False
        self.ruby_system = ruby_system
        self.connectQueues(ruby_system)

    def getBlockSizeBits(self, system):
        bits = int(math.log(system.cache_line_size, 2))
        if 2**bits != system.cache_line_size.value:
            panic("Cache line size not a power of 2!")
        return bits

    def sendEvicts(self, cpu):
        """True if the CPU model or ISA requires sending evictions from caches
           to the CPU. Two scenarios warrant forwarding evictions to the CPU:
           1. The O3 model must keep the LSQ coherent with the caches
           2. The x86 mwait instruction is built on top of coherence
           3. The local exclusive monitor in ARM systems
        """
        if type(cpu) is DerivO3CPU or \
           buildEnv['TARGET_ISA'] in ('x86', 'arm'):
            return True
        return False

    def connectQueues(self, ruby_system):
        """Connect all of the queues for this controller.
        """
        self.mandatoryQueue = MessageBuffer()
        self.requestFromL1Cache = MessageBuffer()
        self.requestFromL1Cache.out_port = ruby_system.network.in_port
        self.responseFromL1Cache = MessageBuffer()
        self.responseFromL1Cache.out_port = ruby_system.network.in_port
        self.unblockFromL1Cache = MessageBuffer()
        self.unblockFromL1Cache.out_port = ruby_system.network.in_port

        self.optionalQueue = MessageBuffer()

        self.requestToL1Cache = MessageBuffer()
        self.requestToL1Cache.in_port = ruby_system.network.out_port
        self.responseToL1Cache = MessageBuffer()
        self.responseToL1Cache.in_port = ruby_system.network.out_port
        
class L2Cache(L2Cache_Controller):
    
    _version = 0
    @classmethod
    def versionCount(cls):
        cls._version += 1 # Use count for this particular type
        return cls._version - 1

    def __init__(self, system, ruby_system, num_l2Caches):
        
        super(L2Cache, self).__init__()

        self.version = self.versionCount()
        # This is the cache memory object that stores the cache data and tags
        self.L2cache = RubyCache(size = '1 MB',
                                assoc = 16,
                                start_index_bit = self.getBlockSizeBits(system, num_l2Caches))
        
        self.transitions_per_cycle = '4'
        self.ruby_system = ruby_system
        self.connectQueues(ruby_system)

    def getBlockSizeBits(self, system, num_l2caches):
        l2_bits = int(math.log(num_l2caches, 2))
        bits = int(math.log(system.cache_line_size, 2)) + l2_bits
        return bits


    def connectQueues(self, ruby_system):
        """Connect all of the queues for this controller.
        """
        self.DirRequestFromL2Cache = MessageBuffer()
        self.DirRequestFromL2Cache.out_port = ruby_system.network.in_port
        self.L1RequestFromL2Cache = MessageBuffer()
        self.L1RequestFromL2Cache.out_port = ruby_system.network.in_port
        self.responseFromL2Cache = MessageBuffer()
        self.responseFromL2Cache.out_port = ruby_system.network.in_port
        self.unblockToL2Cache = MessageBuffer()
        self.unblockToL2Cache.in_port = ruby_system.network.out_port
        self.L1RequestToL2Cache = MessageBuffer()
        self.L1RequestToL2Cache.in_port = ruby_system.network.out_port
        self.responseToL2Cache = MessageBuffer()
        self.responseToL2Cache.in_port = ruby_system.network.out_port   

       

class DirController(Directory_Controller):

    _version = 0
    @classmethod
    def versionCount(cls):
        cls._version += 1 # Use count for this particular type
        return cls._version - 1

    def __init__(self, ruby_system, ranges, mem_ctrls):
        """ranges are the memory ranges assigned to this controller.
        """
        if len(mem_ctrls) > 1:
            panic("This cache system can only be connected to one mem ctrl")
        super(DirController, self).__init__()
        self.version = self.versionCount()
        self.addr_ranges = ranges
        self.ruby_system = ruby_system
        self.directory = RubyDirectoryMemory()
        # Connect this directory to the memory side.
        self.memory_out_port = mem_ctrls[0].port
        self.connectQueues(ruby_system)

    def connectQueues(self, ruby_system):
        self.requestToDir = MessageBuffer()
        self.requestToDir.in_port = ruby_system.network.out_port
        self.responseToDir = MessageBuffer()
        self.responseToDir.in_port = ruby_system.network.out_port
        self.responseFromDir = MessageBuffer()
        self.responseFromDir.out_port = ruby_system.network.in_port
        self.requestToMemory = MessageBuffer()
        self.responseFromMemory = MessageBuffer()

class DMAController(DMA_Controller):

    _version = 0
    @classmethod
    def versionCount(cls):
        cls._version += 1 # Use count for this particular type
        return cls._version - 1

    def __init__(self, ruby_system):
        super(DMAController, self).__init__()
        self.version = self.versionCount()
        self.ruby_system = ruby_system
        self.connectQueues(ruby_system)

    def connectQueues(self, ruby_system):
        self.mandatoryQueue = MessageBuffer()
        self.responseFromDir = MessageBuffer(ordered = True)
        self.responseFromDir.in_port = ruby_system.network.out_port
        self.requestToDir = MessageBuffer()
        self.requestToDir.out_port = ruby_system.network.in_port


class MyNetwork(SimpleNetwork):
    """A simple point-to-point network. This doesn't not use garnet.
    """

    def __init__(self, ruby_system):
        super(MyNetwork, self).__init__()
        self.netifs = []
        self.ruby_system = ruby_system

    def connectControllers(self, controllers):
        """Connect all of the controllers to routers and connec the routers
           together in a point-to-point network.
        """
        # Create one router/switch per controller in the system
        self.routers = [Switch(router_id = i) for i in range(len(controllers))]

        # Make a link from each controller to the router. The link goes
        # externally to the network.
        self.ext_links = [SimpleExtLink(link_id=i, ext_node=c,
                                        int_node=self.routers[i])
                          for i, c in enumerate(controllers)]

        # Make an "internal" link (internal to the network) between every pair
        # of routers.
        link_count = 0
        self.int_links = []
        for ri in self.routers:
            for rj in self.routers:
                if ri == rj: continue # Don't connect a router to itself!
                link_count += 1
                self.int_links.append(SimpleIntLink(link_id = link_count,
                                                    src_node = ri,
                                                    dst_node = rj))
