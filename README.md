


L1 Cache Simulator with MESI Coherence - Execution Plan
This plan outlines the structure, classes, file organization, and core logic for the C++ cache simulator.
1. Project Goal:
Simulate a quad-core processor system with private L1 data caches using the MESI coherence protocol. Process memory access traces, calculate performance metrics, and allow configuration of cache parameters.
2. Core Concepts:
Multi-Core: 4 independent cores executing traces concurrently.
Private L1 Caches: Each core has its own L1 data cache. No L2 cache.
MESI Protocol: Maintain cache coherence using Modified, Exclusive, Shared, Invalid states.
Write-Back, Write-Allocate: Caches update main memory only when a dirty block is evicted. Writes that miss fetch the block first.
LRU Replacement: Least Recently Used policy for eviction within a set.
Blocking Caches: A core halts on a cache miss until the data arrives. Snooping continues.
Shared Bus: Cores/caches communicate via a shared bus, requiring arbitration.
Cycle-Accurate Simulation: Track time in discrete cycles, applying specified latencies.
3. Input & Output:
Input:
Command-line arguments: -t <trace_base>, -s <index_bits>, -E <associativity>, -b <block_bits>, -o <outfile>, -h.
Trace files: <trace_base>_proc{0,1,2,3}.trace. Each line: R/W <hex_address>.
Output:
Statistics printed to console (and optionally <outfile>).
Metrics per core: Read/Write count, Total cycles, Idle cycles, Miss rate, Evictions, Writebacks.
Global metrics: Bus invalidations, Bus data traffic (bytes).
Help message via -h.
4. File Structure:
.
├── src/                     # Source code directory
│   ├── main.cpp             # Entry point, cmdline parsing, simulation setup & run
│   ├── Simulator.h          # Simulator class definition
│   ├── Simulator.cpp        # Simulator class implementation
│   ├── Cache.h              # Cache, CacheSet, CacheLine class definitions
│   ├── Cache.cpp            # Cache, CacheSet, CacheLine class implementations
│   ├── Core.h               # Core class definition
│   ├── Core.cpp             # Core class implementation
│   ├── Bus.h                # Bus class definition
│   ├── Bus.cpp              # Bus class implementation
│   ├── Types.h              # Common definitions (address_t, enums for MESI, Ops)
│   └── TraceReader.h        # Helper for reading/parsing traces
│   └── TraceReader.cpp      # Implementation for TraceReader
├── Makefile                 # Build rules
├── README.md                # Instructions to compile and run
└── report/                  # Directory for the report
    └── report.tex           # LaTeX source for the report
    └── report.pdf           # Compiled report PDF
Use code with caution.
5. Class Design:
Types.h
typedef uint32_t address_t;
typedef uint64_t cycle_t;
enum class CacheLineState { MODIFIED, EXCLUSIVE, SHARED, INVALID };
enum class BusRequestType { BusRd, BusRdX, BusUpd, WriteBack, None }; // BusUpd might not be needed if Write->S uses BusRdX
enum class MemOperation { READ, WRITE };
struct TraceEntry { MemOperation op; address_t addr; };
struct Statistics { /* Counters as described in output */ };
CacheLine (Cache.h, Cache.cpp)
Members:
CacheLineState state = CacheLineState::INVALID;
address_t tag = 0;
cycle_t lastUsedCycle = 0; // For LRU
bool valid = false; // Can be inferred from state != INVALID
Methods:
Constructor
getState(), setState()
getTag(), setTag()
isValid()
updateLRU(cycle_t currentCycle)
getLastUsedCycle()
CacheSet (Cache.h, Cache.cpp)
Members:
std::vector<CacheLine> lines;
int associativity;
Methods:
Constructor(int E)
findLine(address_t tag): Returns pointer to CacheLine or nullptr if miss.
findLRULine(): Returns index of the LRU line for eviction.
getLine(int index): Access a specific line.
Cache (Cache.h, Cache.cpp)
Members:
int id; // Core ID this cache belongs to
int numSets;
int associativity;
int blockSize;
int indexBits;
int blockOffsetBits;
std::vector<CacheSet> sets;
Bus* bus; // Pointer to the shared bus
Statistics stats; // Local cache stats
bool blocked = false; // Is cache waiting for bus/memory?
cycle_t readyCycle = 0; // Cycle when cache is free after miss handling
Methods:
Constructor(int id, int s, int E, int b, Bus* bus)
access(cycle_t currentCycle, MemOperation op, address_t addr): Main entry point from Core. Returns true if hit (1 cycle), false if miss (initiates miss handling).
handleMiss(cycle_t currentCycle, MemOperation op, address_t addr, address_t tag, int setIndex): Initiates bus request for a miss.
snoop(cycle_t currentCycle, BusRequestType busReq, address_t addr): Processes a bus transaction snooped from the bus. Returns state/data if needed. Updates local line state. Returns bool indicating if it responded/intervened.
findBlock(address_t addr): Checks if address is present, returns pointer to line or nullptr.
allocateBlock(cycle_t currentCycle, address_t addr, CacheLineState newState): Finds victim (LRU), handles writeback if dirty, allocates new block.
updateState(address_t addr, CacheLineState newState): Changes the state of a specific block.
getStats(): Returns collected statistics.
isBlocked(), setBlocked(), getReadyCycle()
extractTag(address_t addr), extractIndex(address_t addr), extractOffset(address_t addr): Address decomposition helpers.
calculateAddressBits(): Helper called in constructor.
Core (Core.h, Core.cpp)
Members:
int id;
Cache* cache; // Pointer to its L1 cache
TraceReader* traceReader;
bool finished = false;
bool blocked = false; // Is core waiting for cache?
cycle_t totalCycles = 0;
cycle_t idleCycles = 0;
uint64_t instructionCount = 0;
uint64_t readCount = 0;
uint64_t writeCount = 0;
Methods:
Constructor(int id, Cache* cache, const std::string& tracePath)
tick(cycle_t currentCycle): Executes one step for the core in the current cycle. Reads trace, calls cache->access(). Updates state (blocked/finished) and stats.
isFinished()
isBlocked()
getStats() // Consolidate core-specific stats
TraceReader (TraceReader.h, TraceReader.cpp)
Members:
std::ifstream fileStream;
bool eof = false;
Methods:
Constructor(const std::string& filename)
getNextTrace(TraceEntry& entry): Reads and parses the next line. Returns false if EOF.
isEOF()
Bus (Bus.h, Bus.cpp)
Members:
std::vector<Cache*> caches; // Pointers to all caches for snooping
struct BusTransaction { int requesterId; BusRequestType type; address_t address; cycle_t startCycle; cycle_t completionCycle; bool dataReady = false; /* other fields? */ };
std::deque<BusTransaction> requestQueue; // Pending requests
BusTransaction currentTransaction; // Transaction being processed
bool busy = false;
cycle_t busyUntilCycle = 0;
uint64_t totalDataTraffic = 0; // In bytes
uint64_t totalInvalidations = 0;
int roundRobinArbiter = 0; // Simple arbitration state
int blockSizeBytes;
Methods:
Constructor(int blockSize)
addCache(Cache* cache)
pushRequest(int requesterId, BusRequestType type, address_t address, cycle_t currentCycle): Adds request to queue.
tick(cycle_t currentCycle): Main bus logic per cycle. Arbitrates, starts new transaction if free, checks completion, broadcasts snoops.
broadcastSnoop(cycle_t currentCycle, const BusTransaction& transaction): Sends snoop request to all other caches. Collects responses (e.g., who has it in M/E/S state).
calculateCompletionTime(cycle_t currentCycle, const BusTransaction& transaction, bool suppliedByCache): Determines when data will be ready based on type and source (memory/cache).
notifyRequester(cycle_t currentCycle, const BusTransaction& transaction): Signals the requesting cache that data is ready/transaction complete.
getStats(): Returns bus-related stats.
Simulator (Simulator.h, Simulator.cpp)
Members:
cycle_t currentCycle = 0;
Bus bus;
std::vector<Core> cores;
std::vector<Cache> caches;
std::string traceBaseName;
int numCores = 4; // Fixed for this problem
// Cache parameters (s, E, b)
Methods:
Constructor(const std::string& traceBase, int s, int E, int b)
initialize(): Creates Cores, Caches, Bus, TraceReaders. Connects them.
run(): The main simulation loop. Calls tick() on cores and bus until all cores are finished.
printStats(const std::string& outfile): Collects stats from all components and prints them in the required format.
tick(): Advances simulation by one cycle. Calls bus.tick(), then core.tick() for each core. Increments currentCycle.
checkFinished(): Returns true if all cores isFinished().
6. Simulation Loop Logic (Simulator::run and Simulator::tick):
// Simplified Pseudocode for Simulator::run()
run() {
    initialize();
    while (!checkFinished()) {
        tick();
    }
    printStats();
}

// Simplified Pseudocode for Simulator::tick()
tick() {
    // 1. Process Bus: Handle completions, start new transactions
    bus.tick(currentCycle); // This involves arbitration, snooping broadcasts, calculating latencies

    // 2. Process Cores: Execute instructions if not blocked
    for (Core& core : cores) {
        if (!core.isFinished()) {
            core.tick(currentCycle); // Tries to execute one instruction
            // Core might become blocked here if cache access results in a miss
            // Cache miss handling might place a request on the bus queue inside core.tick() -> cache.access() -> cache.handleMiss() -> bus.pushRequest()
        }
    }

    // 3. Advance Time
    currentCycle++;

    // Update core cycle counts (total/idle) based on whether they could execute
    for(Core& core : cores) {
        if (!core.isFinished()) {
            core.totalCycles = currentCycle; // Update total time spent
            if (core.isBlocked()) {
                 core.idleCycles++;
            }
        }
    }
}
Use code with caution.
C++
7. MESI State Transitions (Implemented within Cache::access and Cache::snoop):
Local Read:
Hit (M, E, S): State unchanged. Update LRU. (1 cycle)
Miss: Issue BusRd. Stall core.
On data return: If another cache supplied (BusRd hit in M/E/S -> data provided, maybe directly 2N cycles, state S), state -> S. If memory supplied (no other cache had it or only S state), state -> E (or S if specified). Update LRU. Unblock core.
Local Write:
Hit (M): State M. Update LRU. (1 cycle)
Hit (E): State -> M. Update LRU. (1 cycle)
Hit (S): Issue BusRdX (or BusUpd - assuming BusRdX per analysis). Stall core.
On completion: State -> M. Update LRU. Unblock core.
Miss: Issue BusRdX. Stall core.
On data return: State -> M. Update LRU. Unblock core.
Snoop BusRd:
Hit (M): Supply data (if protocol dictates, 2N cycles). State -> S. Record data traffic.
Hit (E): State -> S. (Memory supplies data, 100 cycles).
Hit (S): State S. (Memory supplies data, 100 cycles).
Miss (I): Do nothing.
Snoop BusRdX:
Hit (M): Supply data (if protocol dictates, 2N cycles). State -> I. Record invalidation. Record data traffic.
Hit (E): State -> I. Record invalidation. (Memory supplies data, 100 cycles).
Hit (S): State -> I. Record invalidation. (Memory supplies data, 100 cycles).
Miss (I): Do nothing.
8. Latency Implementation:
Cache Hit: 1 cycle (handled within Cache::access). Core proceeds next cycle.
Miss (Bus Interaction):
Core blocks immediately.
Cache places request on Bus::requestQueue.
Bus::tick arbitrates, selects a request.
Bus broadcasts snoop. Caches respond in the same cycle (simplification).
Bus determines source (Memory or Cache M/E).
Bus calculates completionCycle based on latency (100 for memory, 2N for cache block transfer).
Bus sets busyUntilCycle.
When currentCycle == completionCycle, bus notifies requesting cache.
Cache updates state, allocates block (handling victim writeback if needed - adds 100 cycles of bus traffic, potentially delaying other requests, but maybe not the current core's unblocking time directly unless bus is occupied by writeback when fetch completes).
Cache unblocks itself and its core. Core proceeds on the next cycle after unblocking.
Eviction Writeback: If LRU victim is M, issue WriteBack request to bus. Adds 100 cycles of bus occupancy (totalDataTraffic += B).
9. Assumptions to Document:
Bus Arbitration: Assume simple round-robin.
Snoop Response: Assumed instantaneous within the bus cycle where the snoop is broadcast.
Cache-to-Cache Transfer: If BusRd/BusRdX hits in state M, that cache supplies the block directly (2N cycles latency). Otherwise, memory supplies (100 cycles latency).
Write to S: Issues BusRdX, waits for data transfer (100 or 2N cycles) to gain exclusive ownership.
Writeback Timing: A dirty eviction adds 100 cycles of bus traffic, potentially delaying other requests. The eviction itself doesn't add stall time to the miss that caused it, beyond the fetch latency, but it uses the bus resource.
Bus Bandwidth: Bus handles one transaction "phase" per cycle (request grant OR data transfer progression). A 100-cycle memory access occupies the bus data path for that duration conceptually, preventing other data transfers but maybe not new requests/snoops if modeled granularly (simplification: assume bus busy for full latency).
BusUpdate: Explicit 2-cycle BusUpdate message not used; coherence achieved via BusRd/BusRdX.
10. Implementation Steps:
Setup project structure, Makefile.
Define Types.h.
Implement CacheLine, CacheSet. Test basic functionality.
Implement TraceReader. Test reading sample lines.
Implement address decomposition logic (in Cache or Types).
Implement basic Cache structure (members, constructor, getters).
Implement Bus structure (members, constructor, addCache).
Implement Core structure (members, constructor).
Implement Simulator structure (members, constructor, initialize basic objects).
Implement main.cpp argument parsing.
Implement Cache::access logic for Hits (M, E, S).
Implement Cache::findBlock, Cache::updateLRU.
Implement basic Simulator loop structure (run, tick, checkFinished).
Implement Core::tick to read trace and call Cache::access. Handle core blocking/unblocking stub.
Implement Cache::handleMiss to create BusTransaction.
Implement Bus::pushRequest.
Implement Bus::tick arbitration logic.
Implement Bus::tick snoop broadcast logic (Cache::snoop stub).
Implement Cache::snoop MESI state transitions based on bus requests.
Implement Bus::tick latency calculation and completion check.
Implement Bus::tick notifying requester (Cache needs method to handle completion).
Implement Cache logic to handle miss completion (allocate block, update state, unblock core).
Implement LRU victim selection and dirty writeback logic (Cache::allocateBlock, issue WriteBack via Bus).
Implement Statistics tracking in all components.
Implement Simulator::printStats.
Thorough testing and debugging with simple cases, then provided traces.
Add experimental code (varying parameters, max execution time).
Write report.
This detailed plan provides a roadmap for building the simulator, addressing the key requirements and complexities involved.




Analysis of Grading Scheme:
High Importance Metrics:
Total Execution Cycles (Core): 0.20 weight, T=0.05 tolerance. Very important, requires accurate latency modeling, bus contention, and blocking.
Idle Cycles (Core): 0.10 weight, T=0.05 tolerance. Directly related to execution cycles and blocking.
Data Traffic (Bytes) (Core & Bus): 0.10 (Core) + 0.50 * 0.2 (Bus) = 0.20 combined weight. T=0.05 tolerance. Requires tracking all block transfers correctly (Memory->Cache, Cache->Cache, Cache->Memory).
Total Bus Transactions (Bus): 0.50 * 0.2 = 0.10 weight. T=0.05 tolerance. Need to count every granted bus request.
Bus Invalidations (Core): 0.10 weight, T=0.10 tolerance. Requires correct MESI state transitions on snoops.
Cache Miss Rate (Core): 0.10 weight, T=0.05 tolerance. Requires accurate hit/miss detection.
Exact Match Metrics:
Total Instructions, Total Reads, Total Writes (Core): Combined 0.20 weight, T=0.01 tolerance. This implies trace reading and counting must be perfect.
Moderate Importance/Tolerance:
Cache Evictions, Writebacks (Core): Combined 0.10 weight, T=0.10 tolerance. Depend on LRU and dirty state tracking.
Modifications/Emphasis for the Execution Plan:
Based on this, the execution plan is generally sound, but we should add emphasis and slightly refine certain aspects:
Statistics Structure/Classes:
Action: Explicitly define the members needed in the statistics structures within the plan.
Core Statistics: Must track instructionCount, readCount, writeCount, totalCycles, idleCycles.
Cache Statistics: Must track cacheMisses, cacheEvictions, writebacks. (Miss rate is calculated later).
Bus Statistics: Must track totalBusTransactions, totalDataTrafficBytes, totalInvalidations. Correction: Invalidations are often counted per-cache when they happen, but the grading scheme lists "Bus Invalidations" per core. This is slightly ambiguous. A common interpretation is "invalidations received by this core's cache due to bus activity". Let's assume Cache::snoop increments a local numInvalidations counter for its cache when its own state changes to I due to a BusRdX. The printStats function will then sum these up if a global bus invalidation count is needed, or report per core as listed. The plan's Bus class should still track totalDataTrafficBytes and totalBusTransactions. We'll add numInvalidationsReceived to the Cache stats.
Instruction Counting (Core::tick):
Emphasis: Highlight that the TraceReader must parse correctly, and Core::tick must increment instructionCount, readCount, or writeCount exactly once per trace line processed before any potential blocking. The T=0.01 tolerance leaves no room for error here.
Cycle Counting (Simulator::tick, Core::tick):
Refinement: Be more precise in the plan:
Core::totalCycles should be the final value of Simulator::currentCycle when the simulation ends (assuming all cores must finish).
Core::idleCycles should be incremented within Simulator::tick (or Core::tick) for each cycle where core.isBlocked() is true and core.isFinished() is false.
Data Traffic Calculation (Bus::tick, Cache::snoop, Cache::allocateBlock):
Emphasis: Explicitly state that totalDataTrafficBytes on the Bus must be incremented by blockSizeBytes (calculated as 1 << b) for:
Every Memory->Cache transfer (BusRd/BusRdX miss served by memory).
Every Cache->Cache transfer (BusRd/BusRdX miss served by another cache in M state - requires 2N cycle latency).
Every Cache->Memory transfer (Writeback on eviction).
The 2-cycle BusUpdate latency mentioned in the problem spec seems contradictory to the 2N block transfer latency. Assumption: Stick to the block transfer latencies (100 cycles mem, 2N cycles cache-to-cache) for data movement and traffic calculation, as these align better with BusRd/BusRdX/Writeback operations. Ignore the 2-cycle word transfer unless specifically implementing BusUpd separately (which the plan currently doesn't).
Bus Transaction Counting (Bus::tick):
Action: Add uint64_t totalBusTransactions counter to Bus. Increment it once every time the bus grants access and starts processing any request (BusRd, BusRdX, WriteBack).
Invalidation Counting (Cache::snoop):
Refinement: Add uint64_t numInvalidationsReceived to Cache statistics. Increment this counter inside Cache::snoop whenever the snooped transaction (BusRdX) causes the current cache line to transition to the INVALID state.
Latency Implementation (Bus::calculateCompletionTime):
Emphasis: Reiterate the need to use the correct latencies: 1 cycle (hit), +100 (mem fetch), +2N (cache-to-cache transfer, where N = words/block = blockSizeBytes / 4), +100 (writeback bus time). The core stall duration depends on the fetch latency (100 or 2N).
Tolerance Consideration: While implementing, keep the tolerances in mind. For T=0.05 metrics, minor variations in arbitration or timing edge cases might be acceptable, but the core logic (MESI states, latencies) must be sound. For T=0.01, implementation must be exact.
Updated Plan Snippets (Conceptual):
*   **`Cache` (`Cache.h`, `Cache.cpp`)**
    *   **Members:**
        *   ... // Other members
        *   struct CacheStats {
            uint64_t cacheMisses = 0;
            uint64_t cacheEvictions = 0;
            uint64_t writebacks = 0;
            uint64_t numInvalidationsReceived = 0; // Incremented on Snoop(BusRdX) -> I
            uint64_t accesses = 0; // Needed for miss rate
        } stats;
        *   ... // Other members
    *   **Methods:**
        *   ...
        *   `access(...)`: Increment `stats.accesses`. If miss, increment `stats.cacheMisses`.
        *   `snoop(...)`: If state goes to I due to BusRdX, increment `stats.numInvalidationsReceived`.
        *   `allocateBlock(...)`: If victim evicted, increment `stats.cacheEvictions`. If victim was M, increment `stats.writebacks` and issue Bus WB.
        *   `getMissRate()`: return `stats.cacheMisses / (double)stats.accesses;` (handle division by zero).

*   **`Core` (`Core.h`, `Core.cpp`)**
    *   **Members:**
        *   ...
        *   struct CoreStats {
            uint64_t instructionCount = 0; // Total accesses attempted
            uint64_t readCount = 0;
            uint64_t writeCount = 0;
            cycle_t totalCycles = 0; // Final simulator cycle count
            cycle_t idleCycles = 0; // Cycles core was blocked
        } stats;
    *   **Methods:**
        *   `tick(...)`: Read trace. Increment `stats.instructionCount` and `read/writeCount` *exactly*. Call cache. Update `blocked` status.
        *   `incrementIdleCycle()`: Called by simulator if core is blocked this cycle.

*   **`Bus` (`Bus.h`, `Bus.cpp`)**
    *   **Members:**
        *   ...
        *   uint64_t totalDataTrafficBytes = 0;
        *   uint64_t totalBusTransactions = 0; // Count granted requests
        *   int blockSizeBytes; // Calculated from b
    *   **Methods:**
        *   `tick(...)`: When granting a request: increment `totalBusTransactions`. Calculate completion time and bus busy time. During data transfer phase or completion: increment `totalDataTrafficBytes` by `blockSizeBytes` if it's a BusRd/BusRdX data fetch or a Writeback.
        *   `calculateCompletionTime(...)`: Use 100 (mem), 2 * (blockSizeBytes / 4) (cache-to-cache).
        *   `getBusStats()`: Return traffic and transaction counts.

*   **`Simulator` (`Simulator.h`, `Simulator.cpp`)**
    *   **Methods:**
        *   `tick()`: ... // Bus tick, Core ticks
            For each core `c`:
                If `!c.isFinished()`:
                    If `c.isBlocked()`:
                        `c.incrementIdleCycle()`
            `currentCycle++`
        *   `run()`: Loop until all cores finished. After loop, set `core.stats.totalCycles = currentCycle` for all cores.
        *   `printStats()`: Gather stats from all `Core`, `Cache`, `Bus`. Calculate miss rates. Print according to format. Handle per-core "Bus Invalidations" by reporting `cache.stats.numInvalidationsReceived`. Handle per-core "Data Traffic" (this seems odd in grading scheme, usually bus traffic is global. *Assumption:* Report the global `bus.totalDataTrafficBytes / 4` for each core, or clarify if per-core traffic tracking is really needed - likely just report global bus traffic). *Clarification needed on "Data Traffic (Bytes)" per core weight.* Assuming it contributes to the overall bus traffic metric which is then weighted. The output requires specific per-core stats and bus summary stats. Let's report `numInvalidationsReceived` per core, and report `totalBusTransactions` and `totalDataTrafficBytes` in the Bus Summary section.

These refinements align the execution plan more closely with the specific metrics and tolerances outlined in the grading scheme, emphasizing the critical areas for accuracy. The ambiguity around per-core "Data Traffic" and "Bus Invalidations" reporting needs clarification, but the plan follows a reasonable interpretation.