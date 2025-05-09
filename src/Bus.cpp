#include "Bus.h"
#include "Cache.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>

Bus::Bus(int blockSize) : 
    busy(false), 
    busyUntilCycle(0), 
    roundRobinArbiter(0),
    blockSizeBytes(1 << blockSize),
    totalDataTrafficBytes(0),
    totalBusTransactions(0) {
    std::cout << "DEBUG: Bus initialized with block size: " << blockSizeBytes << " bytes" << std::endl;
    std::cout << "DEBUG: Memory latency set to: " << memoryLatency << " cycles" << std::endl;
}

void Bus::addCache(Cache* cache) {
    caches.push_back(cache);
    std::cout << "DEBUG: Added cache " << cache->getId() << " to bus" << std::endl;
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
    
    std::cout << "DEBUG: Cycle " << currentCycle << ": Core " << requesterId 
              << " pushed " << getBusRequestTypeString(type) << " request for address 0x" 
              << std::hex << address << std::dec 
              << " to bus queue (queue size: " << requestQueue.size() << ")" << std::endl;
}

size_t Bus::getQueueSize() const {
    return requestQueue.size();
}

size_t Bus::findHighestPriorityRequest() const {
    // Skip this if there's only one request in the queue
    if (requestQueue.size() == 1) {
        return 0;
    }
    
    // Phase 1: Group requests by type and find highest priority type
    BusRequestType highestPriority = BusRequestType::None;
    for (const auto& req : requestQueue) {
        if (static_cast<int>(req.type) > static_cast<int>(highestPriority)) {
            highestPriority = req.type;
        }
    }
    
    // Phase 2: Collect all request indices with the highest priority type
    std::vector<size_t> sameTypeIndices;
    for (size_t i = 0; i < requestQueue.size(); i++) {
        if (requestQueue[i].type == highestPriority) {
            sameTypeIndices.push_back(i);
        }
    }
    
    // If only one request of highest priority, return its index
    if (sameTypeIndices.size() == 1) {
        return sameTypeIndices[0];
    }
    
    // Phase 3: Apply round-robin selection among same-priority requests
    
    // Store requester IDs and their corresponding queue indices
    std::vector<std::pair<int, size_t>> requesterIndices;
    for (size_t i = 0; i < sameTypeIndices.size(); i++) {
        size_t queueIdx = sameTypeIndices[i];
        requesterIndices.push_back({requestQueue[queueIdx].requesterId, queueIdx});
    }
    
    // Sort by requester ID for deterministic selection
    std::sort(requesterIndices.begin(), requesterIndices.end());
    
    // Find the first requester with ID >= roundRobinArbiter
    for (const auto& pair : requesterIndices) {
        if (pair.first >= roundRobinArbiter) {
            return pair.second;
        }
    }
    
    // If we get here, wrap around to the lowest ID
    return requesterIndices[0].second;
}

void Bus::tick(cycle_t currentCycle) {
    // If there's an ongoing transaction, check if it's complete
    if (busy && currentCycle >= busyUntilCycle) {
        std::cout << "DEBUG: Cycle " << currentCycle << ": Bus transaction completed for Core " 
                  << currentTransaction.requesterId << ", addr: 0x" 
                  << std::hex << currentTransaction.address << std::dec 
                  << ", type: " << getBusRequestTypeString(currentTransaction.type) 
                  << ", served by cache: " << (currentTransaction.servedByCache ? "yes" : "no") 
                  << std::endl;
                  
        // Current transaction is complete
        notifyRequester(currentCycle, currentTransaction);
        busy = false;
        
        // Update round robin arbiter position after completing a transaction
        // This ensures fair scheduling for future requests
        roundRobinArbiter = (currentTransaction.requesterId + 1) % caches.size();
    }
    
    // If bus is free and there are pending requests, start a new transaction
    if (!busy && !requestQueue.empty()) {
        // Find highest priority request
        size_t bestIndex = findHighestPriorityRequest();
        
        // Process that request
        currentTransaction = requestQueue[bestIndex];
        requestQueue.erase(requestQueue.begin() + bestIndex);
        
        // Broadcast to all caches except requester
        bool suppliedByCache = broadcastSnoop(currentCycle, currentTransaction);
        
        // Calculate completion time
        cycle_t completionCycle = calculateCompletionTime(currentCycle, currentTransaction, suppliedByCache);
        
        currentTransaction.completionCycle = completionCycle;
        currentTransaction.servedByCache = suppliedByCache;
        
        // Set bus state
        busy = true;
        busyUntilCycle = completionCycle;
        totalBusTransactions++;
        
        // Update data traffic statistics
        // A bus transaction carries data in the following cases:
        // 1. BusRd or BusRdX that is served by cache (cache-to-cache transfer)
        // 2. BusRd or BusRdX that is served by memory (memory-to-cache transfer)
        // 3. WriteBack (cache-to-memory transfer)
        if (currentTransaction.type == BusRequestType::BusRd || 
            currentTransaction.type == BusRequestType::BusRdX ||
            currentTransaction.type == BusRequestType::WriteBack) {
            totalDataTrafficBytes += blockSizeBytes;
        }
        
        std::cout << "DEBUG: Cycle " << currentCycle << ": Bus started transaction for Core " 
                  << currentTransaction.requesterId << ", addr: 0x" 
                  << std::hex << currentTransaction.address << std::dec 
                  << ", type: " << getBusRequestTypeString(currentTransaction.type) 
                  << ", will complete at cycle " << completionCycle 
                  << " (latency: " << (completionCycle - currentCycle) << " cycles)"
                  << ", served by cache: " << (suppliedByCache ? "yes" : "no") 
                  << std::endl;
    }
}

bool Bus::broadcastSnoop(cycle_t currentCycle, const BusTransaction& transaction) {
    bool suppliedByCache = false;
    
    std::cout << "DEBUG: Cycle " << currentCycle << ": Broadcasting snoop for addr 0x" 
              << std::hex << transaction.address << std::dec 
              << ", type: " << getBusRequestTypeString(transaction.type) << std::endl;
    
    // Send snoop to all caches except requester
    for (size_t i = 0; i < caches.size(); i++) {
        if (static_cast<int>(i) != transaction.requesterId) {
            bool responded = caches[i]->snoop(currentCycle, transaction.type, transaction.address);
            if (responded) {
                suppliedByCache = true;
                std::cout << "DEBUG: Cycle " << currentCycle << ": Cache " << i 
                          << " responded to snoop" << std::endl;
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
            std::cout << "DEBUG: Cache-to-cache transfer latency: " << latency 
                      << " cycles (block size: " << blockSizeBytes 
                      << " bytes, " << wordsPerBlock << " words)" << std::endl;
        } else {
            // Memory access: 100 cycles
            latency = memoryLatency;
            std::cout << "DEBUG: Memory access latency: " << latency << " cycles" << std::endl;
        }
    } else if (transaction.type == BusRequestType::WriteBack) {
        // Writeback to memory: 100 cycles
        latency = memoryLatency;
        std::cout << "DEBUG: WriteBack latency: " << latency << " cycles" << std::endl;
    }
    
    return currentCycle + latency;
}

void Bus::notifyRequester(cycle_t currentCycle, const BusTransaction& transaction) {
    // Handle WriteBack case first
    if (transaction.type == BusRequestType::WriteBack) {
        // For WriteBack, just need to notify cache that writeback is complete
        std::cout << "DEBUG: Cycle " << currentCycle << ": WriteBack completed for Core " 
                  << transaction.requesterId << std::endl;
                  
        // A writeback doesn't invalidate or change the state of the line
        // The line was already evicted, so we're just notifying that the transaction completed
        caches[transaction.requesterId]->notifyTransactionComplete(
            currentCycle, transaction.address, CacheLineState::INVALID);
        return;
    }
    
    // For read/write requests, determine the appropriate new state for the block
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
        std::cerr << "ERROR: Invalid transaction type in notifyRequester: " 
                  << static_cast<int>(transaction.type) << std::endl;
        return;
    }
    
    std::cout << "DEBUG: Cycle " << currentCycle << ": Notifying Core " 
              << transaction.requesterId << " that transaction is complete, "
              << "new state: " << getCacheLineStateString(newState) << std::endl;
    
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

// Helper for debugging
std::string Bus::getBusRequestTypeString(BusRequestType type) const {
    switch (type) {
        case BusRequestType::BusRd: return "BusRd";
        case BusRequestType::BusRdX: return "BusRdX";
        case BusRequestType::WriteBack: return "WriteBack";
        case BusRequestType::None: return "None";
        default: return "Unknown";
    }
}

// Helper for debugging
std::string Bus::getCacheLineStateString(CacheLineState state) const {
    switch (state) {
        case CacheLineState::MODIFIED: return "Modified";
        case CacheLineState::EXCLUSIVE: return "Exclusive";
        case CacheLineState::SHARED: return "Shared";
        case CacheLineState::INVALID: return "Invalid";
        default: return "Unknown";
    }
} 
