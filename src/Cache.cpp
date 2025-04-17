#include "Cache.h"
#include "Bus.h"
#include <iostream>
#include <cmath>

// CacheLine Implementation
CacheLine::CacheLine() : tag(0), lastUsedCycle(0) {
    flags.state = 0;     // Invalid state
    flags.valid = 0;     // Not valid
}

CacheLineState CacheLine::getState() const {
    return static_cast<CacheLineState>(flags.state);
}

void CacheLine::setState(CacheLineState newState) {
    flags.state = static_cast<unsigned>(newState);
    flags.valid = (newState != CacheLineState::INVALID);
}

address_t CacheLine::getTag() const {
    return tag;
}

void CacheLine::setTag(address_t newTag) {
    tag = newTag;
}

bool CacheLine::isValid() const {
    return flags.valid;
}

bool CacheLine::isDirty() const {
    return flags.state == static_cast<unsigned>(CacheLineState::MODIFIED);
}

cycle_t CacheLine::getLastUsedCycle() const {
    return lastUsedCycle;
}

void CacheLine::updateLRU(cycle_t currentCycle) {
    lastUsedCycle = currentCycle;
}


// CacheSet Implementation
CacheSet::CacheSet(int E) : associativity(E) {
    lines.resize(E);
    tagToLineIndex.reserve(E);  // Reserve space for max entries
}

CacheLine* CacheSet::findLine(address_t tag) {
    // First check the lookup table
    auto it = tagToLineIndex.find(tag);
    if (it != tagToLineIndex.end() && lines[it->second].isValid()) {
        return &lines[it->second];
    }
    return nullptr;
}

void CacheSet::updateLookupTable(address_t tag, int index) {
    tagToLineIndex[tag] = index;
}

void CacheSet::removeLookupEntry(address_t tag) {
    tagToLineIndex.erase(tag);
}

int CacheSet::findLRULine() const {
    int lruIndex = 0;
    cycle_t oldestCycle = lines[0].getLastUsedCycle();

    for (int i = 1; i < associativity; i++) {
        if (lines[i].getLastUsedCycle() < oldestCycle) {
            oldestCycle = lines[i].getLastUsedCycle();
            lruIndex = i;
        }
    }

    return lruIndex;
}

CacheLine& CacheSet::getLine(int index) {
    return lines[index];
}

int CacheSet::getAssociativity() const {
    return associativity;
}

// Cache Implementation
Cache::Cache(int id, int s, int E, int b, Bus* bus) 
    : id(id), 
      numSets(1 << s), 
      associativity(E), 
      blockSize(1 << b), 
      indexBits(s), 
      blockOffsetBits(b),
      bus(bus),
      blocked(false),
      readyCycle(0) {
    
    // Precompute address manipulation masks and shifts
    tagMask = ~((1ULL << (indexBits + blockOffsetBits)) - 1);
    tagShift = indexBits + blockOffsetBits;
    indexMask = ((1ULL << indexBits) - 1) << blockOffsetBits;
    indexShift = blockOffsetBits;
    offsetMask = (1ULL << blockOffsetBits) - 1;

    // Initialize sets
    sets.reserve(numSets);
    for (int i = 0; i < numSets; i++) {
        sets.emplace_back(associativity);
    }
    
    // Initialize statistics
    stats = {0, 0, 0, 0, 0, 0};
}

bool Cache::access(cycle_t currentCycle, MemOperation op, address_t addr) {
    // Increment access counter
    stats.accesses++;
    
    // Extract address components
    address_t tag = extractTag(addr);
    int setIndex = extractIndex(addr);
    
    // Look for the block in the cache
    CacheLine* line = sets[setIndex].findLine(tag);
    
    if (line != nullptr) {
        // Cache hit
        stats.hits++;
        
        // Update LRU status
        line->updateLRU(currentCycle);
        
        // Handle based on operation and current state
        if (op == MemOperation::READ) {
            // Read hit - no state change needed
            return true;
        } else { // Write operation
            if (line->getState() == CacheLineState::MODIFIED) {
                // Already in M state - no change needed
                return true;
            } else if (line->getState() == CacheLineState::EXCLUSIVE) {
                // Exclusive -> Modified
                line->setState(CacheLineState::MODIFIED);
                return true;
            } else { // Shared state
                // Need to gain exclusive ownership - issue BusRdX
                handleMiss(currentCycle, op, addr, tag, setIndex);
                return false; // Block the core
            }
        }
    } else {
        // Cache miss
        stats.misses++;
        
        // Handle miss - initiate memory transaction
        handleMiss(currentCycle, op, addr, tag, setIndex);
        return false; // Block the core
    }
}

void Cache::handleMiss(cycle_t currentCycle, MemOperation op, address_t addr, address_t tag, int setIndex) {
    // Block cache while handling miss
    blocked = true;
    
    // Determine transaction type based on operation
    BusRequestType requestType;
    if (op == MemOperation::READ) {
        requestType = BusRequestType::BusRd;
    } else { // Write
        requestType = BusRequestType::BusRdX;
    }
    
    // Issue request to the bus
    bus->pushRequest(id, requestType, addr, currentCycle);
}

void Cache::allocateBlock(cycle_t currentCycle, address_t addr, CacheLineState newState) {
    address_t tag = extractTag(addr);
    int setIndex = extractIndex(addr);
    
    // Find victim using LRU policy
    int victimIndex = sets[setIndex].findLRULine();
    CacheLine& victimLine = sets[setIndex].getLine(victimIndex);
    
    // Handle writeback if victim is dirty
    if (victimLine.isValid()) {
        // Remove old tag from lookup table
        sets[setIndex].removeLookupEntry(victimLine.getTag());
        // Eviction counter
        stats.evictions++;
        
        if (victimLine.isDirty()) {
            // Need to write back to memory
            stats.writebacks++;
            
            // Reconstruct victim address (tag + index + 0s for offset)
            address_t victimTag = victimLine.getTag();
            address_t victimAddr = (victimTag << (indexBits + blockOffsetBits)) | 
                                  (setIndex << blockOffsetBits);
            
            // Issue writeback transaction to bus
            bus->pushRequest(id, BusRequestType::WriteBack, victimAddr, currentCycle);
        }
    }
    
    // Update victim line with new block
    victimLine.setTag(tag);
    victimLine.setState(newState);
    victimLine.updateLRU(currentCycle);
    
    // Update lookup table with new mapping
    sets[setIndex].updateLookupTable(tag, victimIndex);
}

bool Cache::snoop(cycle_t currentCycle, BusRequestType busReq, address_t addr) {
    // Find if we have this block
    CacheLine* line = findBlock(addr);
    
    // If we don't have the block, nothing to do
    if (line == nullptr) {
        return false;
    }
    
    bool responded = false;
    
    // Handle based on request type and current state
    if (busReq == BusRequestType::BusRd) {
        // Another cache wants to read
        if (line->getState() == CacheLineState::MODIFIED) {
            // Need to supply data and change to Shared
            line->setState(CacheLineState::SHARED);
            responded = true;
        } else if (line->getState() == CacheLineState::EXCLUSIVE) {
            // Change to Shared (memory will supply data)
            line->setState(CacheLineState::SHARED);
        }
        // Shared state remains Shared, no action needed
    } else if (busReq == BusRequestType::BusRdX) {
        // Another cache wants exclusive access
        if (line->getState() != CacheLineState::INVALID) {
            // Need to invalidate our copy
            if (line->getState() == CacheLineState::MODIFIED) {
                // If in M state, we need to supply data
                responded = true;
            }
            line->setState(CacheLineState::INVALID);
            stats.invalidationsReceived++;
        }
    }
    
    return responded;
}

CacheLine* Cache::findBlock(address_t addr) {
    address_t tag = extractTag(addr);
    int setIndex = extractIndex(addr);
    
    return sets[setIndex].findLine(tag);
}

void Cache::updateState(address_t addr, CacheLineState newState) {
    CacheLine* line = findBlock(addr);
    if (line != nullptr) {
        line->setState(newState);
    }
}

void Cache::notifyTransactionComplete(cycle_t currentCycle, address_t addr, CacheLineState newState) {
    // Allocate the block with the appropriate state
    allocateBlock(currentCycle, addr, newState);
    
    // Unblock the cache
    blocked = false;
    readyCycle = currentCycle + 1; // Ready from next cycle
}

// Address manipulation helpers
/*
address_t Cache::extractTag(address_t addr) const {
    return addr >> (indexBits + blockOffsetBits);
}

int Cache::extractIndex(address_t addr) const {
    return (addr >> blockOffsetBits) & ((1 << indexBits) - 1);
}

int Cache::extractOffset(address_t addr) const {
    return addr & ((1 << blockOffsetBits) - 1);
}
*/

address_t Cache::extractTag(address_t addr) const {
    return (addr & tagMask) >> tagShift;
}

int Cache::extractIndex(address_t addr) const {
    return (addr & indexMask) >> indexShift;
}

int Cache::extractOffset(address_t addr) const {
    return addr & offsetMask;
}

// Getters and setters
bool Cache::isBlocked() const {
    return blocked;
}

void Cache::setBlocked(bool b) {
    blocked = b;
}

cycle_t Cache::getReadyCycle() const {
    return readyCycle;
}

void Cache::setReadyCycle(cycle_t cycle) {
    readyCycle = cycle;
}

// Cache parameters
int Cache::getCacheSize() const {
    return numSets * associativity * blockSize;
}

int Cache::getBlockSize() const {
    return blockSize;
}

int Cache::getAssociativity() const {
    return associativity;
}

int Cache::getNumSets() const {
    return numSets;
}

// Statistics functions
double Cache::getMissRate() const {
    if (stats.accesses == 0) return 0.0;
    return static_cast<double>(stats.misses) / stats.accesses;
}

uint64_t Cache::getAccesses() const {
    return stats.accesses;
}

uint64_t Cache::getHits() const {
    return stats.hits;
}

uint64_t Cache::getMisses() const {
    return stats.misses;
}

uint64_t Cache::getEvictions() const {
    return stats.evictions;
}

uint64_t Cache::getWritebacks() const {
    return stats.writebacks;
}

uint64_t Cache::getInvalidationsReceived() const {
    return stats.invalidationsReceived;
} 
