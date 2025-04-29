#ifndef BUS_H
#define BUS_H

#include <vector>
#include <deque>
#include "Types.h"

// Forward declaration of Cache class to avoid circular dependencies
class Cache;

// Bus class for shared communication between caches
class Bus {
private:
    // Transaction structure for bus requests
    struct BusTransaction {
        int requesterId;               // ID of requesting cache
        BusRequestType type;           // Type of request
        address_t address;             // Memory address
        cycle_t startCycle;            // Cycle when transaction started
        cycle_t completionCycle;       // Cycle when transaction will complete
        bool dataReady;                // Is data ready for the requester?
        bool servedByCache;            // Was this request served by another cache?
        bool dataAvailableInMemory; // Is data available in memory?
    };

    std::vector<Cache*> caches;        // Connected caches
    std::deque<BusTransaction> requestQueue; // Pending requests
    BusTransaction currentTransaction; // Transaction being processed
    
    bool busy;                         // Is bus currently handling a transaction?
    cycle_t busyUntilCycle;            // Cycle when bus will be free
    int roundRobinArbiter;             // Simple arbitration stateS

    int blockSizeBytes;                // Size of cache block in bytes
    
    // Statistics
    uint64_t totalDataTrafficBytes;    // Total data transferred on bus (bytes)
    uint64_t totalBusTransactions;     // Total number of bus transactions
    
    // Helper methods
    bool broadcastSnoop(cycle_t currentCycle, const BusTransaction& transaction);
    cycle_t calculateCompletionTime(cycle_t currentCycle, const BusTransaction& transaction, bool suppliedByCache);
    void notifyRequester(cycle_t currentCycle, const BusTransaction& transaction);

    size_t findHighestPriorityRequest() const; // Find the highest priority request in the queue
    
    // Check if a transaction needs writeback before proceeding
    bool needsWriteback(const BusTransaction& transaction, Cache** ownerCache) const;
    
    // Helper method to handle transaction completion
    void completeTransaction(cycle_t currentCycle);
    
    // Helper method to process writebacks
    void handleWriteback(cycle_t currentCycle, address_t address, int sourceId);

    const int busLatency = 10;      // Bus transfer latency in cycles
    const int memoryLatency = 100;  // Memory access latency in cycles

public:
    // Constructor takes block size in bytes
    Bus(int blockSize);
    
    // Register a cache to the bus
    void addCache(Cache* cache);
    
    // Push a new request to the bus queue
    void pushRequest(int requesterId, BusRequestType type, address_t address, cycle_t currentCycle);
    
    // Process one cycle of bus activity
    void tick(cycle_t currentCycle);
    
    // Get size of request queue
    size_t getQueueSize() const;
    
    // Check if bus has pending transactions
    bool hasTransactionsInProgress() const;
    
    // Get statistics
    uint64_t getTotalDataTrafficBytes() const;
    uint64_t getTotalBusTransactions() const;
    
    // Get block size
    int getBlockSizeBytes() const;
};

#endif // BUS_H