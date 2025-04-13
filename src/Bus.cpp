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

void Bus::tick(cycle_t currentCycle) {
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
                // but for simulation correctness, we want to make sure all caches update their state
            }
        }
    }
    
    return suppliedByCache;
}

cycle_t Bus::calculateCompletionTime(cycle_t currentCycle, const BusTransaction& transaction, bool suppliedByCache) {
    cycle_t latency = 0;
    
    // Calculate latency based on transaction type and source
    if (transaction.type == BusRequestType::BusRd || 
        transaction.type == BusRequestType::BusRdX) {
        if (suppliedByCache) {
            // Cache-to-cache transfer: 2N cycles (N words per block)
            int wordsPerBlock = blockSizeBytes / 4; // 4 bytes per word
            latency = 2 * wordsPerBlock;
        } else {
            // Memory access: 100 cycles
            latency = 100;
        }
    } else if (transaction.type == BusRequestType::WriteBack) {
        // Writeback to memory: 100 cycles
        latency = 100;
    }
    
    return currentCycle + latency;
}

void Bus::notifyRequester(cycle_t currentCycle, const BusTransaction& transaction) {
    // Determine the appropriate new state for the block
    CacheLineState newState;
    
    if (transaction.type == BusRequestType::BusRd) {
        // If another cache has the block, it goes to Shared
        // Otherwise, it goes to Exclusive
        newState = transaction.servedByCache ? 
            CacheLineState::SHARED : CacheLineState::EXCLUSIVE;
    } else if (transaction.type == BusRequestType::BusRdX) {
        // Always goes to Modified
        newState = CacheLineState::MODIFIED;
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