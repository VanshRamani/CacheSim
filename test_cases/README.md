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

### 2. Ping-Pong Effect
**Files**: `pingpong_core[0-3].trace`

This test demonstrates the ping-pong effect where two cores repeatedly write to the same address (0x200):
- Core 0 and Core 1 repeatedly read and write to address 0x200
- Core 2 and Core 3 operate on a different address (0x300)

**Why it's interesting**: This creates a situation where the cache line containing address 0x200 constantly bounces between Core 0 and Core 1's caches, causing excessive invalidations and bus traffic.

### 3. Cache-to-Cache Transfer
**Files**: `c2c_core[0-3].trace`

This test demonstrates cache-to-cache transfers, where one core gets data from another core's cache rather than from memory:
- Core 0 reads and writes to address 0x400
- Core 1 and Core 2 later read from address 0x400
- Various other addresses are accessed by different cores

**Why it's interesting**: The MESI protocol allows sharing of clean data between caches without memory access, optimizing performance. This test shows the expected state transitions (M→S in Core 0, I→S in other cores).

### 4. Hot Spot Contention
**Files**: `hotspot_core[0-3].trace`

This test demonstrates hot spot contention, where all cores try to access and modify the same memory location:
- All four cores read and write to address 0x500

**Why it's interesting**: This creates extreme contention for a single cache line, resulting in a large number of invalidations and state transitions. This is often a severe performance bottleneck in parallel applications.

### 5. Private vs Shared Data
**Files**: `private_shared_core[0-3].trace`

This test contrasts private data access with shared data access:
- Each core has its own private address (0x600, 0x610, 0x620, 0x630)
- All cores also access a shared address (0x700)

**Why it's interesting**: This demonstrates how mixing private and shared data affects cache behavior. Private data should remain in Exclusive or Modified state without invalidations, while shared data will experience state transitions and potential invalidations.

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