# L1 Cache Simulator with MESI Coherence Protocol

This project implements a simulator for L1 data caches in a quad-core processor system with MESI cache coherence protocol support.

## Features

- Simulates 4 cores with private L1 data caches
- Configurable cache parameters (sets, associativity, block size)
- MESI cache coherence protocol
- Write-back, write-allocate policy
- LRU replacement policy
- Cycle-accurate simulation
- Detailed statistics reporting

## Requirements

- C++11 compatible compiler (g++ recommended)
- Make
- Linux/Unix environment (for the getopt library)

## Compiling

To compile the simulator, simply run:

```
make
```

This will create an executable called `L1simulate`.

## Usage

The simulator takes the following command-line parameters:

```
./L1simulate [options]
Options:
  -t <tracefile>: name of parallel application (e.g. app1) whose 4 traces are to be used
  -s <s>: number of set index bits (number of sets in the cache = S = 2^s)
  -E <E>: associativity (number of cache lines per set)
  -b <b>: number of block bits (block size = B = 2^b)
  -o <outfilename>: logs output in file for plotting etc.
  -h: prints this help
```

### Example

To run the simulator with the default parameters on the app1 trace:

```
./L1simulate -t app1 -s 7 -E 2 -b 5
```

This will simulate a cache with 128 sets (2^7), 2-way set associativity, and 32-byte (2^5) blocks.

### Input Trace Files

The simulator expects trace files named as:
- `<tracefile>_proc0.trace`
- `<tracefile>_proc1.trace`
- `<tracefile>_proc2.trace`
- `<tracefile>_proc3.trace`

Each trace file should contain memory reference instructions, one per line, in the format:
```
R 0x7e1ac04c
W 0x7e1afe78
```
Where:
- R/W indicates Read/Write operation
- The hexadecimal address is the memory location accessed

### Output

The simulator outputs detailed statistics for each core and the overall system, including:
- Number of read/write instructions per core
- Total execution cycles per core
- Number of idle cycles per core
- Data cache miss rate for each core
- Number of cache evictions per core
- Number of writebacks per core
- Number of invalidations on the bus
- Amount of data traffic (in bytes) on the bus

## Assumptions

1. Memory addresses are 32-bit. If any address is less than 32 bit, remaining MSBs are assumed to be 0.
2. Each memory reference accesses 32-bit (4-bytes) of data (word size is 4 bytes).
3. L1 data caches are backed up directly by main memory (no L2 cache).
4. Initially all caches are empty.
5. Bus arbitration uses round-robin policy.
6. L1 cache hit takes 1 cycle, memory access takes 100 cycles, and cache-to-cache transfer takes 2N cycles (where N is the number of words per block).
7. Caches are blocking - if there is a cache miss, the cache cannot process further requests from the processor core.

## Implementation Details

The simulator is implemented with the following main components:

1. **CacheLine** - Represents a single cache line with MESI state and LRU tracking
2. **CacheSet** - A set of cache lines with the same index
3. **Cache** - The full cache structure for a core with access and snoop functionality
4. **Core** - Simulates a processor core executing memory instructions
5. **Bus** - Manages the shared bus for cache coherence communication
6. **Simulator** - Coordinates the overall simulation and tracks statistics

## Cleaning Up

To clean the build files:

```
make clean
``` 