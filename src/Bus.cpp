#include "Bus.h"
#include "Cache.h"
#include <iostream>

Bus::Bus(int blockSize) : 
    busy(false), 
    busyUntilCycle(0), 
    roundRobinArbiter(0),
    blockSizeBytes(1 << blockSize),
    totalDataTrafficBytes(0),
    totalBusTransactions(0) {}

void Bus::addCache(Cache* cache) {
    caches.push_back(cache);
}

void Bus::pushRequest(int requesterId, BusRequestType type, address_t address, cycle_t currentCycle) {
    // Create a new transaction and add it to the queue
    // Note: This only enqueues the request - it will be processed in a future cycle
    // This separation ensures that all cores' actions within a cycle are treated
    // as if they occurred simultaneously
    BusTransaction transaction;
    transaction.requesterId = requesterId;
    transaction.type = type;
    transaction.address = address;
    transaction.startCycle = currentCycle;
    transaction.completionCycle = 0; // Will be calculated later
    transaction.dataReady = false;
    transaction.servedByCache = false;

    requestQueue.push_back(transaction);
}

size_t Bus::getQueueSize() const {
    return requestQueue.size();
}

size_t Bus::findHighestPriorityRequest() const {
    size_t bestIndex = 0;
    BusRequestType highestPriority = BusRequestType::None;
    
    for (size_t i = 0; i < requestQueue.size(); i++) {
        if (static_cast<int>(requestQueue[i].type) > static_cast<int>(highestPriority)) {
            highestPriority = requestQueue[i].type;
            bestIndex = i;
        }
    }
    
    return bestIndex;
}

void Bus::tick(cycle_t currentCycle) {
    if (!busy && !requestQueue.empty()) {
        // Find highest priority request
        size_t bestIndex = findHighestPriorityRequest();
        
        // Process that request
        currentTransaction = requestQueue[bestIndex];
        requestQueue.erase(requestQueue.begin() + bestIndex);
        
        // Set bus state
        busy = true;
        busyUntilCycle = currentCycle + memoryLatency;
        totalBusTransactions++;
        
        // Broadcast to all caches except requester
        for (Cache* cache : caches) {
            if (cache->getId() != currentTransaction.requesterId) {
                cache->snoop(currentCycle, currentTransaction.type, currentTransaction.address);
            }
        }
    }
    // Process one cycle of bus activity
    // This includes:
    // 1. Completing any ongoing transaction if it's done
    // 2. Starting a new transaction from the queue if the bus is free
    // 3. Updating arbitration state
    
    // If bus is busy, check if current transaction is complete
    if (busy && currentCycle >= busyUntilCycle) {
        // Current transaction is complete
        notifyRequester(currentCycle, currentTransaction);
        busy = false;
    }
    
    // If bus is not busy, try to start a new transaction
    if (!busy && !requestQueue.empty()) {
        // Start processing the next request
        currentTransaction = requestQueue.front();
        requestQueue.pop_front();
        
        // Track transaction count
        totalBusTransactions++;
        
        // Broadcast snoop to all caches
        bool suppliedByCache = broadcastSnoop(currentCycle, currentTransaction);
        
        // Calculate when this transaction will complete
        cycle_t completionCycle = calculateCompletionTime(
            currentCycle, currentTransaction, suppliedByCache);
        
        currentTransaction.completionCycle = completionCycle;
        currentTransaction.servedByCache = suppliedByCache;
        
        // Mark bus as busy until completion
        busy = true;
        busyUntilCycle = completionCycle;
        
        // Update data traffic statistics
        // Every bus transaction carries data equal to block size
        totalDataTrafficBytes += blockSizeBytes;
    }
    
    // Update round robin arbiter for next cycle
    if (!requestQueue.empty()) {
        roundRobinArbiter = (roundRobinArbiter + 1) % caches.size();
    }
}

bool Bus::broadcastSnoop(cycle_t currentCycle, const BusTransaction& transaction) {
    bool suppliedByCache = false;
    
    // Send snoop to all caches except requester
    for (size_t i = 0; i < caches.size(); i++) {
        if (static_cast<int>(i) != transaction.requesterId) {
            bool responded = caches[i]->snoop(currentCycle, transaction.type, transaction.address);
            if (responded) {
                suppliedByCache = true;
                // In real hardware, we would break here since only one cache can respond,
                // but for simulation correctness, we want to ensure all caches update their state
                // However, for BusRdX, we can stop after the first cache with the block in M state responds
                if (transaction.type == BusRequestType::BusRdX) {
                    // Only one cache can have the block in M state, so we can break
                    break;
                }
            }
        }
    }
    
    return suppliedByCache;
}

cycle_t Bus::calculateCompletionTime(cycle_t currentCycle, const BusTransaction& transaction, bool suppliedByCache) {
    cycle_t latency = 0;
    
    // Calculate latency based on transaction type and source
    if (transaction.type == BusRequestType::BusRd) {
        if (suppliedByCache) {
            // Cache-to-cache transfer: 2N cycles (N words per block) + memory update
            int wordsPerBlock = blockSizeBytes / 4; // 4 bytes per word
            latency = 2 * wordsPerBlock;
            
            // We still need to update memory if the data was in M state in another cache
            // Add a small latency for this, but don't add the full memory latency
            // as the cache-to-cache transfer happens concurrently
            latency += 10;
        } else {
            // Memory access: 100 cycles
            latency = 100;
        }
    } else if (transaction.type == BusRequestType::BusRdX) {
        // For BusRdX, we always access memory because:
        // 1. If no cache has the block, we fetch from memory
        // 2. If another cache has it in M, it must write back first
        // 3. If caches have it in S/E, they invalidate but data comes from memory
        latency = 100;
    } else if (transaction.type == BusRequestType::WriteBack) {
        // Writeback to memory: 100 cycles
        latency = 100;
    }
    
    return currentCycle + latency;
}

void Bus::notifyRequester(cycle_t currentCycle, const BusTransaction& transaction) {
    // First, check if the transaction requires writeback before proceeding
    bool needsWriteback = false;
    Cache* ownerCache = nullptr;
    
    // Check if any cache has the block in modified state
    for (size_t i = 0; i < caches.size(); i++) {
        if (static_cast<int>(i) != transaction.requesterId) {
            CacheLine* line = caches[i]->findBlock(transaction.address);
            if (line != nullptr && line->getState() == CacheLineState::MODIFIED) {
                needsWriteback = true;
                ownerCache = caches[i];
                break;
            }
        }
    }
    
    // Handle writeback if necessary
    if (needsWriteback && ownerCache) {
        // Force writeback to memory first
        // No need to create a new transaction, just add memory access delay
        currentCycle += 100; // Memory writeback latency
    }
    
    // Determine the appropriate new state for the block
    CacheLineState newState;
    
    // Use a local copy of servedByCache
    bool isServedByCache = transaction.servedByCache;
    
    if (transaction.type == BusRequestType::BusRd) {
        // If another cache has the block, it goes to Shared
        // Otherwise, it goes to Exclusive
        newState = isServedByCache ? 
            CacheLineState::SHARED : CacheLineState::EXCLUSIVE;
    } else if (transaction.type == BusRequestType::BusRdX) {
        // Always goes to Modified
        newState = CacheLineState::MODIFIED;
        
        // For BusRdX, we cannot do direct cache-to-cache transfer
        // Data must be written back to memory first if it was modified
        isServedByCache = false;
    } else {
        // For WriteBack, don't need to notify requester about a new state
        return;
    }
    
    // Notify the requesting cache
    caches[transaction.requesterId]->notifyTransactionComplete(
        currentCycle, transaction.address, newState);
}

uint64_t Bus::getTotalDataTrafficBytes() const {
    return totalDataTrafficBytes;
}

uint64_t Bus::getTotalBusTransactions() const {
    return totalBusTransactions;
}

int Bus::getBlockSizeBytes() const {
    return blockSizeBytes;
} 
