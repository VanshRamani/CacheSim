#include "Core.h"
#include "Cache.h"
#include <iostream>

Core::Core(int id, Cache* cache, const std::string& tracePath) :
    id(id),
    cache(cache),
    traceReader(new TraceReader(tracePath)),
    finished(false),
    blocked(false),
    totalCycles(0),
    idleCycles(0),  
    instructionCount(0),
    readCount(0),
    writeCount(0) {}

Core::~Core() {
    delete traceReader;
}

void Core::tick(cycle_t currentCycle) {
    // If core is finished processing its trace
    if (finished) {
        idleCycles++;
        return;
    }

    // If core is blocked waiting for memory
    if (blocked) {
        if (!cache->isBlocked() && currentCycle >= cache->getReadyCycle()) {
            // Cache has completed the operation, unblock core
            blocked = false;
        } else {
            // Still waiting for cache - count as idle cycle
            idleCycles++;
            return;
        }
    }

    // Try to execute next instruction from trace
    TraceEntry entry;
    if (traceReader->getNextTrace(entry)) {
        // Count instruction
        instructionCount++;
        
        // Count read/write
        if (entry.op == MemOperation::READ) {
            readCount++;
        } else {
            writeCount++;
        }

        // Try to access cache
        bool hit = cache->accessCache(currentCycle, entry.op, entry.addr);
        
        if (!hit) {
            // Cache miss - block the core
            blocked = true;
        }
    } else {
        // End of trace - mark core as finished
        finished = true;
        idleCycles++;
    }
}

void Core::incrementIdleCycle() {
    if (!finished) {
        idleCycles++;
    }
}

bool Core::isFinished() const {
    return finished;
}

bool Core::isBlocked() const {
    return blocked;
}

void Core::setTotalCycles(cycle_t cycles) {
    totalCycles = cycles;
}

cycle_t Core::getTotalCycles() const {
    return totalCycles;
}

size_t Core::getIdleCycles() const {
    size_t activeCycles = instructionCount; // Each instruction takes 1 cycle when hit
    
    // Idle cycles = total cycles - active cycles
    // Ensure we don't return a negative value if calculation is off
    return totalCycles > activeCycles ? totalCycles - activeCycles : 0;
}

uint64_t Core::getInstructionCount() const {
    return instructionCount;
}

uint64_t Core::getReadCount() const {
    return readCount;
}

uint64_t Core::getWriteCount() const {
    return writeCount;
}