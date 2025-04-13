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
    // If core is finished, nothing to do
    if (finished) {
        return;
    }

    // If core is blocked, check if cache is still blocked
    if (blocked) {
        if (!cache->isBlocked() && currentCycle >= cache->getReadyCycle()) {
            // Cache has completed the operation, unblock core
            blocked = false;
        } else {
            // Still waiting for cache
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
        bool hit = cache->access(currentCycle, entry.op, entry.addr);
        
        if (!hit) {
            // Cache miss - block the core
            blocked = true;
        }
    } else {
        // End of trace - mark core as finished
        finished = true;
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

cycle_t Core::getIdleCycles() const {
    return idleCycles;
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