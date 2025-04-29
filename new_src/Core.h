#ifndef CORE_H
#define CORE_H

#include "Types.h"
#include "TraceReader.h"
#include <string>

// Forward declaration
class Cache;

/**
 * Core class representing a CPU core that executes memory operations
 * from a trace file and accesses its private cache.
 */
class Core {
private:
    int id;                     // Core ID
    Cache* cache;               // Private cache for this core
    TraceReader* traceReader;   // Trace reader for this core
    
    bool finished;              // Flag indicating if trace is complete
    bool blocked;               // Flag indicating if core is waiting for cache
    
    // Statistics
    cycle_t totalCycles;        // Total execution cycles
    size_t idleCycles;          // Cycles spent idle
    uint64_t instructionCount;  // Total instructions executed
    uint64_t readCount;         // Read operations executed
    uint64_t writeCount;        // Write operations executed

public:
    /**
     * Constructor
     * 
     * @param id Core ID
     * @param cache Pointer to core's private cache
     * @param tracePath Path to trace file
     */
    Core(int id, Cache* cache, const std::string& tracePath);
    
    /**
     * Destructor
     */
    ~Core();
    
    /**
     * Execute one cycle of core operation
     * 
     * @param currentCycle Current simulation cycle
     */
    void tick(cycle_t currentCycle);
    
    /**
     * Increment idle cycle counter
     */
    void incrementIdleCycle();
    
    /**
     * Check if core has finished executing its trace
     * 
     * @return True if trace execution is complete
     */
    bool isFinished() const;
    
    /**
     * Check if core is blocked waiting for cache
     * 
     * @return True if core is blocked
     */
    bool isBlocked() const;
    
    /**
     * Set the total number of execution cycles
     * 
     * @param cycles Total cycle count
     */
    void setTotalCycles(cycle_t cycles);
    
    /**
     * Get the total number of execution cycles
     * 
     * @return Total cycle count
     */
    cycle_t getTotalCycles() const;
    
    /**
     * Get the number of idle cycles
     * 
     * @return Idle cycle count
     */
    size_t getIdleCycles() const;
    
    /**
     * Get the number of instructions executed
     * 
     * @return Instruction count
     */
    uint64_t getInstructionCount() const;
    
    /**
     * Get the number of read operations executed
     * 
     * @return Read operation count
     */
    uint64_t getReadCount() const;
    
    /**
     * Get the number of write operations executed
     * 
     * @return Write operation count
     */
    uint64_t getWriteCount() const;
};

#endif // CORE_H