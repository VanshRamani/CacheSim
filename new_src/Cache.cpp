#include "Cache.h"
#include "Bus.h"
#include <iostream>
#include <cmath>
#include <cstring> // For memcpy

// CacheLine Implementation
CacheLine::CacheLine(int blockSize) : tag(0), lastUsedCycle(0), blockSizeBytes(blockSize) {
    state = CacheLineState::INVALID;
    data.resize(blockSize);
}

CacheLineState CacheLine::getState() const {
    return state;
}

void CacheLine::setState(CacheLineState newState) {
    state = newState;
}

address_t CacheLine::getTag() const {
    return tag;
}

void CacheLine::setTag(address_t newTag) {
    tag = newTag;
}

bool CacheLine::isValid() const {
    return state != CacheLineState::INVALID;
}

bool CacheLine::isDirty() const {
    return state == CacheLineState::MODIFIED;
}

cycle_t CacheLine::getLastUsedCycle() const {
    return lastUsedCycle;
}

void CacheLine::updateLRU(cycle_t currentCycle) {
    lastUsedCycle = currentCycle;
}

const std::vector<uint8_t>& CacheLine::getData() const {
    return data;
}

void CacheLine::setData(const std::vector<uint8_t>& newData) {
    if (newData.size() != data.size()) {
        std::cerr << "Error: Data size mismatch in CacheLine::setData" << std::endl;
        return;
    }
    data = newData;
}

// Cache Implementation
Cache::Cache(int id, int s, int E, int b, Bus* bus)
    : id(id),
    numSets(1 << s),
    associativity(E),
    blockSizeBytes(1 << b),
    indexBits(s),
    blockOffsetBits(b),
    bus(bus),
    blocked(false),
    readyCycle(0),
    cacheLines(numSets, std::vector<CacheLine>(associativity, CacheLine(blockSizeBytes))), // Initialize cacheLines
    lruCounters(numSets, std::vector<cycle_t>(associativity, 0))
{
    // Precompute address manipulation masks and shifts
    tagMask = ~((1ULL << (indexBits + blockOffsetBits)) - 1);
    tagShift = indexBits + blockOffsetBits;
    indexMask = ((1ULL << indexBits) - 1) << blockOffsetBits;
    indexShift = blockOffsetBits;
    offsetMask = (1ULL << blockOffsetBits) - 1;

    // Initialize statistics
    stats = { 0, 0, 0, 0, 0, 0, 0, 0 };
}

int Cache::getId() const {
    return id;
}

bool Cache::isBlocked() const {
    return blocked;
}

cycle_t Cache::getReadyCycle() const {
    return readyCycle;
}

void Cache::setReadyCycle(cycle_t cycle) {
    readyCycle = cycle;
}

bool Cache::accessCache(cycle_t currentCycle, MemOperation op, address_t addr) {
    stats.accesses++;

    address_t tag = extractTag(addr);
    int setIndex = extractIndex(addr);
    // Remove or mark as unused:
    // int offset = extractOffset(addr);
    (void)extractOffset(addr); // Mark as explicitly unused if you want to keep it


    // Find the cache line within the set
    int lineIndex = findLine(setIndex, tag);

    if (lineIndex != -1) {
        // Cache hit
        stats.hits++;
        updateLRU(setIndex, lineIndex, currentCycle);

        if (op == MemOperation::WRITE) {
            // Handle write hit based on MESI state
            switch (cacheLines[setIndex][lineIndex].getState()) {
                case CacheLineState::MODIFIED:
                    // Already modified, just update data
                    break;
                case CacheLineState::EXCLUSIVE:
                    // Upgrade to modified, no bus transaction
                    cacheLines[setIndex][lineIndex].setState(CacheLineState::MODIFIED);
                    break;
                case CacheLineState::SHARED:
                    // Send BusRdX, invalidate other copies
                    bus->pushRequest(id, BusRequestType::BusRdX, addr, currentCycle);
                    blocked = true;
                    readyCycle = currentCycle + 1; // Arbitrary delay, will be updated by bus
                    return false; // Stall
                case CacheLineState::INVALID:
                    break;
            }
        }
        return true; // Hit
    }
    else {
        // Cache miss
        stats.misses++;
        // Handle the miss
        handleMiss(currentCycle, op, addr);
        return false;
    }
}

// In Cache.cpp - handleMiss method
void Cache::handleMiss(cycle_t currentCycle, MemOperation op, address_t addr) {
    // Block cache while handling miss
    blocked = true;
    
    // Determine transaction type based on operation
    BusRequestType requestType;
    if (op == MemOperation::READ) {
        requestType = BusRequestType::BusRd;  // Simple read request
    } else {
        // This is effectively a RWITM (Read With Intent To Modify)
        requestType = BusRequestType::BusRdX;
    }
    
    // Issue request to the bus
    bus->pushRequest(id, requestType, addr, currentCycle);
}

void Cache::handleBusRequest(cycle_t currentCycle, BusRequestType busReq, address_t addr) {
    address_t tag = extractTag(addr);
    int setIndex = extractIndex(addr);
    // Mark unused parameter
    (void)extractOffset(addr); 

    int lineIndex = findLine(setIndex, tag);
    
    // If we don't have the block, nothing to do
    if (lineIndex == -1) return; 

    CacheLineState currentState = cacheLines[setIndex][lineIndex].getState();

    // Update LRU since we're touching this block
    updateLRU(setIndex, lineIndex, currentCycle);

    switch (busReq) {
        case BusRequestType::BusRd:
            // Another cache wants to read this block
            if (currentState == CacheLineState::MODIFIED) {
                // We have the most recent data - need to provide it and update memory
                // Send a flush which will update memory and provide data to the requester
                bus->pushRequest(id, BusRequestType::WriteBack, addr, currentCycle);
                
                // Change our state to Shared since other caches will have a copy now
                cacheLines[setIndex][lineIndex].setState(CacheLineState::SHARED);
            }
            else if (currentState == CacheLineState::EXCLUSIVE) {
                // We have an exclusive clean copy - can share it
                // No need to write back to memory, but we move to Shared state
                cacheLines[setIndex][lineIndex].setState(CacheLineState::SHARED);
            }
            // For Shared state: nothing to do, already in shared state
            // For Invalid state: shouldn't be here since we check lineIndex != -1
            break;

        // In Cache.cpp - handleBusRequest method
        case BusRequestType::BusRdX:
        // Another cache wants exclusive access (for write)
        if (currentState == CacheLineState::MODIFIED) {
            // We have modified data - need to write it back before giving up ownership
            bus->pushRequest(id, BusRequestType::WriteBack, addr, currentCycle);
        }

        // In all valid states (M/E/S), we need to invalidate our copy
        if (currentState != CacheLineState::INVALID) {
            cacheLines[setIndex][lineIndex].setState(CacheLineState::INVALID);
            stats.invalidationsReceived++;
        }
        break;

        case BusRequestType::WriteBack:
            // Someone is writing back to memory
            // If we have a copy, it should already be in the right state
            // We could update our local data if we want to implement cache-to-cache transfers
            // but it's not strictly necessary for protocol correctness
            break;

        default:
            // Unknown request type - do nothing
            break;
    }
}

void Cache::notifyTransactionComplete(cycle_t currentCycle, address_t addr, CacheLineState newState) {
    allocateBlock(currentCycle, addr, newState);
    blocked = false;
    readyCycle = currentCycle + 1;
}

void Cache::allocateBlock(cycle_t currentCycle, address_t addr, CacheLineState newState) {
    address_t tag = extractTag(addr);
    int setIndex = extractIndex(addr);

    int victimIndex = findLRU(setIndex);

    // Writeback if dirty
    if (cacheLines[setIndex][victimIndex].isDirty()) {
        stats.writebacks++;
        bus->pushRequest(id, BusRequestType::WriteBack, addr, currentCycle);
    }
    else if(cacheLines[setIndex][victimIndex].isValid()){
        stats.evictions++;
    }

    // Load new block
    cacheLines[setIndex][victimIndex].setTag(tag);
    cacheLines[setIndex][victimIndex].setState(newState);
    //assume we get data from bus
    std::vector<uint8_t> data(blockSizeBytes);
    cacheLines[setIndex][victimIndex].setData(data);
    updateLRU(setIndex, victimIndex, currentCycle);
}

int Cache::findLine(int setIndex, address_t tag) const {
    for (int i = 0; i < associativity; i++) {
        if (cacheLines[setIndex][i].isValid() && cacheLines[setIndex][i].getTag() == tag) {
            return i;
        }
    }
    return -1;
}

int Cache::findLRU(int setIndex) const {
    int lruIndex = 0;
    cycle_t minTime = lruCounters[setIndex][0];
    for (int i = 1; i < associativity; i++) {
        if (lruCounters[setIndex][i] < minTime) {
            minTime = lruCounters[setIndex][i];
            lruIndex = i;
        }
    }
    return lruIndex;
}

CacheLine* Cache::findBlock(address_t addr) {
    address_t tag = extractTag(addr);
    int setIndex = extractIndex(addr);
    
    int lineIndex = findLine(setIndex, tag);
    if (lineIndex != -1) {
        return &cacheLines[setIndex][lineIndex];
    }
    return nullptr;
}

void Cache::updateLRU(int setIndex, int lineIndex, cycle_t currentCycle) {
    lruCounters[setIndex][lineIndex] = currentCycle;
}

address_t Cache::extractTag(address_t addr) const {
    return (addr & tagMask) >> tagShift;
}

int Cache::extractIndex(address_t addr) const {
    return (addr & indexMask) >> indexShift;
}

int Cache::extractOffset(address_t addr) const {
    return addr & offsetMask;
}

int Cache::getCacheSize() const {
    return numSets * associativity * blockSizeBytes;
}

int Cache::getBlockSize() const {
    return blockSizeBytes;
}

int Cache::getAssociativity() const {
    return associativity;
}

int Cache::getNumSets() const {
    return numSets;
}

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