#ifndef CACHE_H
#define CACHE_H

#include "Types.h"
#include <vector>

// Forward declarations
class Bus;

// Cache line states for MESI protocol
enum class CacheLineState {
    MODIFIED,   // Modified: Line is modified and exclusive
    EXCLUSIVE,  // Exclusive: Line is unmodified and exclusive
    SHARED,     // Shared: Line is unmodified and shared
    INVALID     // Invalid: Line is not valid
};

// Represents a single cache line
class CacheLine {
private:
    address_t tag;              // Tag bits of the address
    CacheLineState state;       // MESI state
    cycle_t lastUsedCycle;      // Last cycle this line was used (for LRU)
    int blockSizeBytes;         // Size of the block in bytes
    std::vector<uint8_t> data;  // Actual data stored in the line

public:
    // Constructor
    CacheLine(int blockSize);

    // State management
    CacheLineState getState() const;
    void setState(CacheLineState newState);

    // Tag management
    address_t getTag() const;
    void setTag(address_t newTag);

    // Status checks
    bool isValid() const;
    bool isDirty() const;

    // LRU management
    cycle_t getLastUsedCycle() const;
    void updateLRU(cycle_t currentCycle);

    // Data access
    const std::vector<uint8_t>& getData() const;
    void setData(const std::vector<uint8_t>& newData);
};

// Statistics for a cache
struct CacheStats {
    uint64_t accesses;              // Total cache accesses
    uint64_t hits;                  // Cache hits
    uint64_t misses;                // Cache misses
    uint64_t evictions;             // Number of evictions
    uint64_t writebacks;            // Number of writebacks
    uint64_t invalidationsReceived; // Number of invalidations received from bus
    uint64_t prefetchRequests;      // Number of prefetch requests issued
    uint64_t usefulPrefetches;      // Number of useful prefetches
};

// Main cache class
class Cache {
private:
    int id;                     // Cache ID
    int numSets;                // Number of sets in cache
    int associativity;          // Number of ways (associativity)
    int blockSizeBytes;         // Block size in bytes
    
    // Address bit fields
    int indexBits;              // Number of bits for index
    int blockOffsetBits;        // Number of bits for block offset
    
    // Masks and shifts for address parsing
    address_t tagMask;
    address_t indexMask;
    address_t offsetMask;
    int tagShift;
    int indexShift;
    
    Bus* bus;                   // Bus for coherence traffic
    
    // Status flags
    bool blocked;               // Is cache blocked waiting for memory?
    cycle_t readyCycle;         // Cycle when cache will be ready again
    
    // Storage
    std::vector<std::vector<CacheLine>> cacheLines;  // 2D array of cache lines
    std::vector<std::vector<cycle_t>> lruCounters;   // LRU counters
    
    // Statistics
    CacheStats stats;
    
    // Helper methods
    void allocateBlock(cycle_t currentCycle, address_t addr, CacheLineState newState);
    int findLine(int setIndex, address_t tag) const;
    int findLRU(int setIndex) const;
    void updateLRU(int setIndex, int lineIndex, cycle_t currentCycle);
    
public:
    // Constructor
    Cache(int id, int s, int E, int b, Bus* bus);
    
    CacheLine* findBlock(address_t addr);
    // Cache access methods
    bool accessCache(cycle_t currentCycle, MemOperation op, address_t addr);
    void handleMiss(cycle_t currentCycle, MemOperation op, address_t addr);
    void handleBusRequest(cycle_t currentCycle, BusRequestType busReq, address_t addr);
    void notifyTransactionComplete(cycle_t currentCycle, address_t addr, CacheLineState newState);
    
    // Status methods
    int getId() const;
    bool isBlocked() const;
    cycle_t getReadyCycle() const;
    void setReadyCycle(cycle_t cycle);
    
    // Address manipulation methods
    address_t extractTag(address_t addr) const;
    int extractIndex(address_t addr) const;
    int extractOffset(address_t addr) const;
    
    // Cache configuration getters
    int getCacheSize() const;
    int getBlockSize() const;
    int getAssociativity() const;
    int getNumSets() const;
    
    // Statistics methods
    double getMissRate() const;
    uint64_t getAccesses() const;
    uint64_t getHits() const;
    uint64_t getMisses() const;
    uint64_t getEvictions() const;
    uint64_t getWritebacks() const;
    uint64_t getInvalidationsReceived() const;
};

#endif // CACHE_H