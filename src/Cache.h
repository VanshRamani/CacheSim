#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include "Types.h"
#include <unordered_map>
#include <deque>

// Forward declaration of Bus class to avoid circular dependencies
class Bus;

// Cache Line - represents a single cache line
class CacheLine {
private:
    address_t tag;
    cycle_t lastUsedCycle;
    struct {
        unsigned state : 2;  // MESI state (2 bits: 00=I, 01=S, 10=E, 11=M)
        unsigned valid : 1;  // Valid bit
    } flags;

public:
    CacheLine();  // Declaration only
    
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
};

// Cache Set - represents a set of cache lines with the same index
class CacheSet {
private:
    std::vector<CacheLine> lines;
    int associativity;
    std::unordered_map<address_t, int> tagToLineIndex;  // New lookup table

public:
    CacheSet(int E);
    
    // Find a line with the given tag
    // Returns pointer to the line if found, nullptr if miss
    CacheLine* findLine(address_t tag);
    
    // Find the least recently used line in the set
    int findLRULine() const;
    
    // Get a specific line by index
    CacheLine& getLine(int index);
    
    // Get number of lines in the set
    int getAssociativity() const;

    void updateLookupTable(address_t tag, int index);
    void removeLookupEntry(address_t tag);
};

// Cache structure for a single core
class Cache {
private:
    // Cache parameters
    int id;                 // Core ID this cache belongs to
    int numSets;            // Number of sets (S = 2^s)
    int associativity;      // Number of lines per set (E)
    int blockSize;          // Size of each block in bytes (B = 2^b)
    int indexBits;          // Number of set index bits (s)
    int blockOffsetBits;    // Number of block offset bits (b)

    void prefetch(cycle_t cycle, address_t addr);
    bool shouldPrefetch(address_t currentAddr, address_t nextAddr) const;

    
    // Cache structure
    std::vector<CacheSet> sets;
    
    // Bus connection
    Bus* bus;
    
    // Cache state
    bool blocked;           // Is cache waiting for a memory transaction?
    cycle_t readyCycle;     // Cycle when cache will be ready after miss handling

    // Address manipulation masks and shifts
    address_t tagMask;
    int tagShift;
    address_t indexMask;
    int indexShift;
    address_t offsetMask;
    
    // Statistics
    struct {
        uint64_t accesses;              // Total cache accesses
        uint64_t hits;                  // Cache hits
        uint64_t misses;                // Cache misses
        uint64_t evictions;             // Number of cache line evictions
        uint64_t writebacks;            // Number of writebacks to memory
        uint64_t invalidationsReceived; // Number of invalidations received from bus
        uint64_t prefetchRequests;
        uint64_t usefulPrefetches;
    } stats;
    
    // Address manipulation helpers
    address_t extractTag(address_t addr) const;
    int extractIndex(address_t addr) const;
    int extractOffset(address_t addr) const;
    
    // Miss handling
    void handleMiss(cycle_t currentCycle, MemOperation op, address_t addr, address_t tag, int setIndex);
    
    // Block allocation
    void allocateBlock(cycle_t currentCycle, address_t addr, CacheLineState newState);
    
public:
    Cache(int id, int s, int E, int b, Bus* bus);
    
    // Main cache access function
    bool access(cycle_t currentCycle, MemOperation op, address_t addr);
    
    // Snoop function to handle coherence
    bool snoop(cycle_t currentCycle, BusRequestType busReq, address_t addr);
    
    // Find a block in the cache
    CacheLine* findBlock(address_t addr);
    
    int getId() const { return id; }  // Return the cache/core ID

    // Update state of a block
    void updateState(address_t addr, CacheLineState newState);
    
    // Handle completion of a memory transaction
    void notifyTransactionComplete(cycle_t currentCycle, address_t addr, CacheLineState newState);
    
    // State getters/setters
    bool isBlocked() const;
    void setBlocked(bool blocked);
    cycle_t getReadyCycle() const;
    void setReadyCycle(cycle_t cycle);
    
    // Get cache parameters
    int getCacheSize() const;
    int getBlockSize() const;
    int getAssociativity() const;
    int getNumSets() const;
    
    // Statistics functions
    double getMissRate() const;
    uint64_t getAccesses() const;
    uint64_t getHits() const;
    uint64_t getMisses() const;
    uint64_t getEvictions() const;
    uint64_t getWritebacks() const;
    uint64_t getInvalidationsReceived() const;
};

#endif // CACHE_H 
