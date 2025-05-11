#include "Core.h"
#include "Cache.h"
#include "Simulator.h"
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
    writeCount(0) {
    DEBUG_PRINT("Core " << id << " initialized with trace file: " << tracePath);
}

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
            DEBUG_PRINT("Cycle " << currentCycle << ": Core " << id << " unblocked");
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

        // Every 1000 instructions, print debug info
        if (instructionCount % 1000 == 0) {
            DEBUG_PRINT("Core " << id << " executed " << instructionCount 
                        << " instructions, " << readCount << " reads, " 
                        << writeCount << " writes");
        }

        // Try to access cache
        bool hit = cache->access(currentCycle, entry.op, entry.addr);
        
        if (!hit) {
            // Cache miss - block the core
            blocked = true;
            DEBUG_PRINT("Cycle " << currentCycle << ": Core " << id 
                        << " blocked due to cache miss, addr: 0x" 
                        << std::hex << entry.addr << std::dec 
                        << ", op: " << (entry.op == MemOperation::READ ? "READ" : "WRITE"));
        }
    } else {
        // End of trace - mark core as finished
        finished = true;
        DEBUG_PRINT("Cycle " << currentCycle << ": Core " << id 
                    << " finished execution after " << instructionCount 
                    << " instructions");
    }
}

void Core::incrementIdleCycle() {
    if (!finished) {
        idleCycles++;
        
        // Debug counter for idle cycles
        if (idleCycles % 1000 == 0) {
            DEBUG_PRINT("Core " << id << " idle cycle count: " 
                        << idleCycles);
        }
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
    DEBUG_PRINT("Core " << id << " final stats - Total cycles: " 
                << totalCycles << ", Idle cycles: " << idleCycles 
                << ", Instructions: " << instructionCount 
                << ", Execution cycles: " << (totalCycles - idleCycles));
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

int Core::getId() const {
    return id;
} 
