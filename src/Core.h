#ifndef CORE_H
#define CORE_H

#include <string>
#include "Types.h"
#include "TraceReader.h"

// Forward declarations
class Cache;

// Core class represents a single processor core
class Core {
private:
    int id;                     // Core ID
    Cache* cache;               // Pointer to this core's L1 cache
    TraceReader* traceReader;   // Pointer to trace reader for this core
    
    bool finished;              // Has core finished processing its trace?
    bool blocked;               // Is core waiting for cache?
    
    // Statistics
    cycle_t totalCycles;        // Total execution cycles
    cycle_t idleCycles;         // Cycles spent idle/blocked
    uint64_t instructionCount;  // Total instructions executed
    uint64_t readCount;         // Total read operations
    uint64_t writeCount;        // Total write operations

public:
    Core(int id, Cache* cache, const std::string& tracePath);
    ~Core();
    
    // Process one cycle for this core
    void tick(cycle_t currentCycle);
    
    // Increment idle cycles counter
    void incrementIdleCycle();
    
    // State getters
    bool isFinished() const;
    bool isBlocked() const;
    
    // Set total cycles
    void setTotalCycles(cycle_t cycles);
    
    // Statistics getters
    cycle_t getTotalCycles() const;
    cycle_t getIdleCycles() const;
    uint64_t getInstructionCount() const;
    uint64_t getReadCount() const;
    uint64_t getWriteCount() const;
};

#endif // CORE_H 
