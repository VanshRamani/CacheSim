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
    CacheLineState oldState = getState();
    
    if (oldState != newState) {
        std::cout << "DEBUG: Cache line state transition: " 
                  << cacheLineStateToString(oldState) << " -> " 
                  << cacheLineStateToString(newState) << std::endl;
    }
    
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
    return getState() == CacheLineState::MODIFIED;
}

cycle_t CacheLine::getLastUsedCycle() const {
    return lastUsedCycle;
}

void CacheLine::updateLRU(cycle_t currentCycle) {
    lastUsedCycle = currentCycle;
}

std::string CacheLine::cacheLineStateToString(CacheLineState state) const {
    switch (state) {
        case CacheLineState::MODIFIED: return "Modified";
        case CacheLineState::EXCLUSIVE: return "Exclusive";
        case CacheLineState::SHARED: return "Shared";
        case CacheLineState::INVALID: return "Invalid";
        default: return "Unknown";
    }
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
    // First, look for any invalid lines - these should be replaced first
    for (int i = 0; i < associativity; i++) {
        if (!lines[i].isValid()) {
            return i;
        }
    }

    // If all lines are valid, find the least recently used one
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
    
    // Initialize all statistics to zero
    stats = {0};  // Zero-initialize all fields
}

bool Cache::access(cycle_t currentCycle, MemOperation op, address_t addr) {
    // Increment access counter
    stats.accesses++;
    
    // Extract address components
    address_t tag = extractTag(addr);
    int setIndex = extractIndex(addr);
    
    std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
              << " access, op: " << (op == MemOperation::READ ? "READ" : "WRITE") 
              << ", addr: 0x" << std::hex << addr << std::dec 
              << " (tag: 0x" << std::hex << tag << std::dec 
              << ", set: " << setIndex << ")" << std::endl;
    
    // Look for the block in the cache
    CacheLine* line = sets[setIndex].findLine(tag);
    
    if (line != nullptr) {
        // Cache hit
        stats.hits++;
        
        CacheLineState oldState = line->getState();
        
        std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
                  << " HIT, line state: " << getCacheLineStateString(oldState) << std::endl;
        
        // Update LRU status
        line->updateLRU(currentCycle);
        
        // Handle based on operation and current state
        if (op == MemOperation::READ) {
            // Read hit - no state change needed
            return true;
        } else { // Write operation
            if (oldState == CacheLineState::MODIFIED) {
                // Already in M state - no change needed
                return true;
            } else if (oldState == CacheLineState::EXCLUSIVE) {
                // Exclusive -> Modified
                std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
                          << " transition E->M on write hit" << std::endl;
                line->setState(CacheLineState::MODIFIED);
                return true;
            } else if (oldState == CacheLineState::SHARED) {
                // Shared -> Need to get exclusive ownership via BusRdX
                // We don't change state here - that will happen when the transaction completes
                std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
                          << " write to shared line, need to invalidate other copies" << std::endl;
                
                // This is a write to a shared line, not a miss
                // Issue BusRdX to invalidate other copies but don't count it as a miss
                blocked = true;
                bus->pushRequest(id, BusRequestType::BusRdX, addr, currentCycle);
                return false; // Block the core
            } else {
                // Invalid state - should not happen on a hit
                std::cerr << "ERROR: Cache " << id << " hit on invalid line, addr: 0x" 
                          << std::hex << addr << std::dec << std::endl;
                return false;
            }
        }
    } else {
        // Cache miss
        stats.misses++;
        
        std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
                  << " MISS, addr: 0x" << std::hex << addr << std::dec << std::endl;
        
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
    
    std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
              << " handling miss, issuing " << getBusRequestTypeString(requestType) 
              << " for addr: 0x" << std::hex << addr << std::dec << std::endl;
    
    // Issue request to the bus
    bus->pushRequest(id, requestType, addr, currentCycle);
}

void Cache::allocateBlock(cycle_t currentCycle, address_t addr, CacheLineState newState) {
    address_t tag = extractTag(addr);
    int setIndex = extractIndex(addr);
    
    // Find victim using LRU policy
    int victimIndex = sets[setIndex].findLRULine();
    CacheLine& victimLine = sets[setIndex].getLine(victimIndex);
    
    std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
              << " allocating block, addr: 0x" << std::hex << addr << std::dec 
              << ", state: " << getCacheLineStateString(newState) << std::endl;
    
    // Check if the line we're about to replace is already the same address/tag
    // If so, we just need to update its state
    if (victimLine.isValid() && victimLine.getTag() == tag) {
        std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
                  << " updating existing line for tag: 0x" << std::hex << tag << std::dec 
                  << ", old state: " << getCacheLineStateString(victimLine.getState())
                  << ", new state: " << getCacheLineStateString(newState) << std::endl;
        
        victimLine.setState(newState);
        victimLine.updateLRU(currentCycle);
        return;
    }
    
    // Handle writeback if victim is dirty and valid
    if (victimLine.isValid()) {
        // Remove old tag from lookup table
        address_t oldTag = victimLine.getTag();
        sets[setIndex].removeLookupEntry(oldTag);
        // Eviction counter
        stats.evictions++;
        
        std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
                  << " evicting line, tag: 0x" << std::hex << oldTag << std::dec 
                  << ", state: " << getCacheLineStateString(victimLine.getState()) << std::endl;
        
        if (victimLine.isDirty()) {
            // Need to write back to memory
            stats.writebacks++;
            
            // Reconstruct victim address (tag + index + 0s for offset)
            address_t victimAddr = (oldTag << (indexBits + blockOffsetBits)) | 
                                  (setIndex << blockOffsetBits);
            
            std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
                      << " initiating writeback, addr: 0x" << std::hex << victimAddr << std::dec << std::endl;
            
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
    
    std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
              << " received snoop, req: " << getBusRequestTypeString(busReq) 
              << ", addr: 0x" << std::hex << addr << std::dec 
              << ", have block: " << (line != nullptr ? "yes" : "no") << std::endl;
    
    // If we don't have the block, nothing to do
    if (line == nullptr) {
        return false;
    }
    
    bool responded = false;
    CacheLineState oldState = line->getState();
    
    // Handle based on request type and current state
    if (busReq == BusRequestType::BusRd) {
        // Another cache wants to read
        if (oldState == CacheLineState::MODIFIED) {
            // Need to supply data and change to Shared
            std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
                      << " serving BusRd from M state, transitioning to S" << std::endl;
            
            // First, need to update memory since our copy was modified
            // This is done implicitly in the simulator - data is transferred to the requestor
            // and memory is updated as part of the transaction
            
            line->setState(CacheLineState::SHARED);
            responded = true; // Data supplied from this cache
        } else if (oldState == CacheLineState::EXCLUSIVE) {
            // When in Exclusive state, we have the only valid copy - need to respond
            // and supply data (memory would be up to date, but cache-to-cache transfer is faster)
            std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
                      << " serving BusRd from E state, transitioning to S" << std::endl;
            
            line->setState(CacheLineState::SHARED);
            responded = true; // Data supplied from this cache
        } else if (oldState == CacheLineState::SHARED) {
            // Shared state remains Shared - we don't supply data (comes from memory)
            std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id
                      << " responding to BusRd, remaining in S state (no data transfer)" << std::endl;
            // We don't set responded = true here, as shared caches don't supply data
            // A cache in Shared state doesn't need to respond as memory is up-to-date
        }
    } else if (busReq == BusRequestType::BusRdX) {
        // Another cache wants exclusive access
        if (oldState != CacheLineState::INVALID) {
            // Need to invalidate our copy
            if (oldState == CacheLineState::MODIFIED) {
                // If in M state, we need to supply data to the requester
                // and ensure memory is updated (implicitly handled in the transaction)
                std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
                          << " serving BusRdX from M state, invalidating" << std::endl;
                responded = true;
            } else {
                std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
                          << " invalidating line due to BusRdX (was " 
                          << getCacheLineStateString(oldState) << ")" << std::endl;
            }
            
            // Update LRU information before invalidating
            line->updateLRU(currentCycle);
            
            // Invalidate the line
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
    std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
              << " transaction complete for addr: 0x" << std::hex << addr << std::dec 
              << ", new state: " << getCacheLineStateString(newState) << std::endl;
    
    // Check if this is a normal memory transaction (BusRd/BusRdX) or a writeback
    if (newState == CacheLineState::INVALID) {
        // This is a writeback completion - no need to allocate a block
        // The block was already evicted, so we just need to unblock the cache
        std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
                  << " writeback complete, no state change needed" << std::endl;
    } else {
        // For BusRd and BusRdX, we need to allocate/update a block
        
        // First check if we already have this block in the cache
        // This handles the case of write to a shared line (S->M transition)
        CacheLine* line = findBlock(addr);
        if (line != nullptr) {
            // We already have this block, just update its state
            std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
                      << " updating existing line state to " << getCacheLineStateString(newState) << std::endl;
            line->setState(newState);
            line->updateLRU(currentCycle);
        } else {
            // Need to allocate a new block
            allocateBlock(currentCycle, addr, newState);
        }
    }
    
    // Unblock the cache
    blocked = false;
    readyCycle = currentCycle + 1; // Ready from next cycle
    
    std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << id 
              << " unblocked, ready from cycle " << readyCycle << std::endl;
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

std::string Cache::getBusRequestTypeString(BusRequestType type) const {
    switch (type) {
        case BusRequestType::BusRd: return "BusRd";
        case BusRequestType::BusRdX: return "BusRdX";
        case BusRequestType::WriteBack: return "WriteBack";
        case BusRequestType::None: return "None";
        default: return "Unknown";
    }
}

std::string Cache::getCacheLineStateString(CacheLineState state) const {
    switch (state) {
        case CacheLineState::MODIFIED: return "Modified";
        case CacheLineState::EXCLUSIVE: return "Exclusive";
        case CacheLineState::SHARED: return "Shared";
        case CacheLineState::INVALID: return "Invalid";
        default: return "Unknown";
    }
} 
