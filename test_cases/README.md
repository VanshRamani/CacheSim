# MESI Cache Coherence Test Cases

This directory contains 5 specialized test cases designed to demonstrate various aspects of cache coherence in multicore systems using the MESI protocol. Each test case is crafted to highlight a specific coherence scenario or potential performance issue.

## Test Case Descriptions

### 1. False Sharing
**Files**: `falsesharing_core[0-3].trace`

This test demonstrates false sharing, where different cores access different data items that happen to be in the same cache line. In this test:
- Core 0 accesses address 0x100
- Core 1 accesses address 0x104
- Core 2 accesses address 0x108
- Core 3 accesses address 0x10C

These addresses are within the same cache line (assuming a typical 32-byte line), causing invalidations despite the cores accessing different variables.

**Why it's interesting**: False sharing is a common performance problem in multicore applications, leading to excessive invalidations and cache misses even when cores are logically accessing different data.

**Analysis of Results**: The output shows significantly high bus invalidations across all cores (3-4 for each core), despite each core accessing its own unique address. Core 1 experienced the highest number of invalidations (4) with a 66.67% miss rate. The total bus traffic (192 bytes) is higher than in most other tests, showing how false sharing amplifies coherence traffic. The high execution cycles for Core 0 (438) and Core 1 (338) demonstrate the performance penalty, as each core keeps invalidating the others' cache lines unnecessarily. This is a perfect example of how seemingly independent work can cause severe cache thrashing.

### 2. Ping-Pong Effect
**Files**: `pingpong_core[0-3].trace`

This test demonstrates the ping-pong effect where two cores repeatedly write to the same address (0x200):
- Core 0 and Core 1 repeatedly read and write to address 0x200
- Core 2 and Core 3 operate on a different address (0x300)

**Why it's interesting**: This creates a situation where the cache line containing address 0x200 constantly bounces between Core 0 and Core 1's caches, causing excessive invalidations and bus traffic.

**Analysis of Results**: The ping-pong effect is clearly visible in the results where Core 0 and Core 1 each have 3 bus invalidations, showing how the cache line bounces between them. Core 0's execution time (320 cycles) and Core 1's (220 cycles) are high considering the small number of instructions, highlighting the performance penalty of this contention. The overall bus traffic (160 bytes) and transaction count (14) demonstrate the coherence overhead. Core 3, which only accesses its own data, still has a 100% miss rate due to bus arbitration priorities, showing how contention on the bus affects all cores.

### 3. Cache-to-Cache Transfer
**Files**: `c2c_core[0-3].trace`

This test demonstrates cache-to-cache transfers, where one core gets data from another core's cache rather than from memory:
- Core 0 reads and writes to address 0x400
- Core 1 and Core 2 later read from address 0x400
- Various other addresses are accessed by different cores

**Why it's interesting**: The MESI protocol allows sharing of clean data between caches without memory access, optimizing performance. This test shows the expected state transitions (M→S in Core 0, I→S in other cores).

**Analysis of Results**: The results clearly show cache-to-cache transfer benefits. Core 0 has 0 invalidations despite Core 1 and Core 2 reading its data, demonstrating successful sharing. Core 1 and 2 show invalidations (1 each) when other cores write to shared data. The most telling insight is that the total bus transactions (8) is the lowest among all tests, showing how MESI's sharing optimization reduces bus traffic. The execution times for Core 1 (218 cycles) and Core 2 (101 cycles) are lower than they would be without cache-to-cache transfer, as they avoid going to memory for every read.

### 4. Hot Spot Contention
**Files**: `hotspot_core[0-3].trace`

This test demonstrates hot spot contention, where all cores try to access and modify the same memory location:
- All four cores read and write to address 0x500

**Why it's interesting**: This creates extreme contention for a single cache line, resulting in a large number of invalidations and state transitions. This is often a severe performance bottleneck in parallel applications.

**Analysis of Results**: The results show the classic hot spot pattern. Cores 0 and 1 experience 2 invalidations each, while Cores 2 and 3 experience 1 each. All cores have identical miss rates (66.67%), showing the fairness of invalidation when all cores contend for the same data. The execution cycles are disproportionate (Core 0: 227, Core 1: 235, Core 2: 1, Core 3: 9), showing how the fixed priority bus arbitration favors Core 0 and disadvantages lower-priority cores. The total bus transactions (10) demonstrate coherence overhead even with this simple access pattern. This highlights how shared, frequently-modified data becomes a scalability bottleneck.

### 5. Private vs Shared Data
**Files**: `private_shared_core[0-3].trace`

This test contrasts private data access with shared data access:
- Each core has its own private address (0x600, 0x610, 0x620, 0x630)
- All cores also access a shared address (0x700)

**Why it's interesting**: This demonstrates how mixing private and shared data affects cache behavior. Private data should remain in Exclusive or Modified state without invalidations, while shared data will experience state transitions and potential invalidations.

**Analysis of Results**: The results clearly show the difference between private and shared data access. While all cores access both private and shared data, the invalidation counts are much lower (1-2 per core) than in the false sharing or ping-pong scenarios. The miss rates are also better (40-60%) because private data accesses don't cause invalidations. Core 1, which only reads the shared data (doesn't write to it), still experiences 2 invalidations due to other cores' writes, showing the coherence impact of shared writes. The execution times (Core 0: 429, Core 1: 421, Core 2: 103, Core 3: 3) reflect the priority-based bus arbitration. This test demonstrates the MESI protocol successfully distinguishing between private data (which remains in E or M state) and shared data (which transitions to S state).

## Running the Tests

Use the provided `run_tests.bat` script to execute all tests:

```
.\run_tests.bat
```

The results will be stored in the `output` directory.

## Analyzing Results

Look for these key metrics in the output:
- Bus invalidations: Shows protocol-enforced coherence actions
- Cache miss rates: Impact on performance
- Data traffic on bus: Communication overhead
- Execution cycles: Overall efficiency

False sharing and ping-pong effects typically show high invalidation counts and bus traffic, while private data should show better efficiency than shared data.

## Key Insights

1. **False sharing produces unnecessary invalidations**: The test shows how even when cores access different variables, placing them in the same cache line causes excessive coherence traffic.

2. **Ping-pong pattern severely degrades performance**: When two cores repeatedly write to the same location, the cache line bounces back and forth, causing both cores to experience high latency.

3. **Cache-to-cache transfers optimize read sharing**: The MESI protocol allows cores to get data directly from other caches, reducing memory access latency for shared reads.

4. **Hot spots create performance bottlenecks**: When all cores access and modify the same data, coherence traffic becomes a significant overhead that limits scalability.

5. **Separating private and shared data improves performance**: Keeping private data isolated reduces invalidations and improves cache utilization.

6. **Bus arbitration policy impacts fairness**: In all tests, the fixed priority arbitration causes lower-priority cores to experience worse performance, showing the importance of arbitration in multi-core systems. 