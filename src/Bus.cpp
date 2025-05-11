#include "Bus.h"
#include "Cache.h"
#include "Simulator.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>

Bus::Bus(int blockSize) : 
    busy(false), 
    busyUntilCycle(0), 
    roundRobinArbiter(0), // This is no longer used for arbitration, but keeping for compatibility
    blockSizeBytes(1 << blockSize),
    totalDataTrafficBytes(0),
    totalBusTransactions(0) {
    DEBUG_PRINT("Bus initialized with block size: " << blockSizeBytes << " bytes");
    DEBUG_PRINT("Memory latency set to: " << memoryLatency << " cycles");
    DEBUG_PRINT("Using fixed priority arbitration (Core 0 highest, Core 3 lowest)");
}

void Bus::addCache(Cache* cache) {
    caches.push_back(cache);
    DEBUG_PRINT("Added cache " << cache->getId() << " to bus");
}

void Bus::pushRequest(int requesterId, BusRequestType type, address_t address, cycle_t currentCycle) {
    // Create a new transaction and add it to the queue
    // This should always add to the queue, even if the bus is busy
    BusTransaction transaction;
    transaction.requesterId = requesterId;
    transaction.type = type;
    transaction.address = address;
    transaction.startCycle = currentCycle;
    transaction.completionCycle = 0; // Will be calculated later
    transaction.dataReady = false;
    transaction.servedByCache = false;

    requestQueue.push_back(transaction);
    
    DEBUG_PRINT("Cycle " << currentCycle << ": Core " << requesterId 
                << " pushed " << getBusRequestTypeString(type) << " request for address 0x" 
                << std::hex << address << std::dec 
                << " to bus queue (queue size: " << requestQueue.size() << ")");
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
    
    // Phase 3: Apply fixed priority scheme - lower core ID always gets higher priority
    
    // Store requester IDs and their corresponding queue indices
    std::vector<std::pair<int, size_t>> requesterIndices;
    for (size_t i = 0; i < sameTypeIndices.size(); i++) {
        size_t queueIdx = sameTypeIndices[i];
        requesterIndices.push_back({requestQueue[queueIdx].requesterId, queueIdx});
    }
    
    // Sort by requester ID to get the lowest core ID (highest priority)
    std::sort(requesterIndices.begin(), requesterIndices.end());
    
    // Return the index of the request from the core with the lowest ID
    return requesterIndices[0].second;
}

void Bus::tick(cycle_t currentCycle) {
    // If there's an ongoing transaction, check if it's complete
    if (busy && currentCycle >= busyUntilCycle) {
        DEBUG_PRINT("Cycle " << currentCycle << ": Bus transaction completed for Core " 
                    << currentTransaction.requesterId << ", addr: 0x" 
                    << std::hex << currentTransaction.address << std::dec 
                    << ", type: " << getBusRequestTypeString(currentTransaction.type) 
                    << ", served by cache: " << (currentTransaction.servedByCache ? "yes" : "no"));
                  
        // Current transaction is complete
        notifyRequester(currentCycle, currentTransaction);
        busy = false;
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
        // Only count non-WriteBack operations as bus transactions
        if (currentTransaction.type != BusRequestType::WriteBack) {
            totalBusTransactions++;
        }
        
        // Update data traffic statistics
        // All transactions that involve data transfers should be counted:
        // 1. BusRd: Data transfer from memory or another cache
        // 2. BusRdX: Data transfer from memory or another cache 
        // 3. WriteBack: Data transfer from cache to memory
        if (currentTransaction.type == BusRequestType::BusRd ||
            currentTransaction.type == BusRequestType::BusRdX ||
            currentTransaction.type == BusRequestType::WriteBack) {
            totalDataTrafficBytes += blockSizeBytes;
            
            DEBUG_PRINT("Cycle " << currentCycle << ": Incrementing data traffic by " 
                        << blockSizeBytes << " bytes for " 
                        << getBusRequestTypeString(currentTransaction.type)
                        << ", total now: " << totalDataTrafficBytes << " bytes");
        }
        
        DEBUG_PRINT("Cycle " << currentCycle << ": Bus started transaction for Core " 
                    << currentTransaction.requesterId << ", addr: 0x" 
                    << std::hex << currentTransaction.address << std::dec 
                    << ", type: " << getBusRequestTypeString(currentTransaction.type) 
                    << ", will complete at cycle " << completionCycle 
                    << " (latency: " << (completionCycle - currentCycle) << " cycles)"
                    << ", served by cache: " << (suppliedByCache ? "yes" : "no"));
    }
}

bool Bus::broadcastSnoop(cycle_t currentCycle, const BusTransaction& transaction) {
    bool suppliedByCache = false;
    
    DEBUG_PRINT("Cycle " << currentCycle << ": Broadcasting snoop for addr 0x" 
                << std::hex << transaction.address << std::dec 
                << ", type: " << getBusRequestTypeString(transaction.type));
    
    // Send snoop to all caches except requester
    for (size_t i = 0; i < caches.size(); i++) {
        if (static_cast<int>(i) != transaction.requesterId) {
            bool responded = caches[i]->snoop(currentCycle, transaction.type, transaction.address);
            
            // Set suppliedByCache to true for:
            // 1. BusRd requests when another cache supplies the data (from M, E, or potentially S)
            // 2. BusRdX requests when another cache has it in M state and supplies the data
            if (responded && (transaction.type == BusRequestType::BusRd || 
                            transaction.type == BusRequestType::BusRdX)) {
                suppliedByCache = true;
                DEBUG_PRINT("Cycle " << currentCycle << ": Cache " << i 
                            << " responded to snoop");
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
    if (transaction.type == BusRequestType::BusRd) {
        if (suppliedByCache) {
            // Cache-to-cache transfer: 2N cycles (N words per block)
            int wordsPerBlock = blockSizeBytes / 4; // 4 bytes per word
            latency = 2 * wordsPerBlock;
            DEBUG_PRINT("Cache-to-cache transfer latency: " << latency 
                        << " cycles (block size: " << blockSizeBytes 
                        << " bytes, " << wordsPerBlock << " words)");
        } else {
            // Memory access: 100 cycles
            latency = memoryLatency;
            DEBUG_PRINT("Memory access latency: " << latency << " cycles");
        }
    } else if (transaction.type == BusRequestType::BusRdX) {
        // For BusRdX, check if data was supplied by another cache (Modified state)
        if (suppliedByCache) {
            // Cache-to-cache transfer: 2N cycles (N words per block)
            int wordsPerBlock = blockSizeBytes / 4; // 4 bytes per word
            latency = 2 * wordsPerBlock;
            DEBUG_PRINT("Cache-to-cache transfer latency (BusRdX): " << latency 
                        << " cycles (block size: " << blockSizeBytes 
                        << " bytes, " << wordsPerBlock << " words)");
        } else {
            // Memory access: 100 cycles
            latency = memoryLatency;
            DEBUG_PRINT("Memory access latency (BusRdX): " << latency << " cycles");
        }
    } else if (transaction.type == BusRequestType::WriteBack) {
        // Writeback to memory: 100 cycles
        latency = memoryLatency;
        DEBUG_PRINT("WriteBack latency: " << latency << " cycles");
    } else if (transaction.type == BusRequestType::InvalidateSig) {
        // Invalidation signal: Fixed latency of 10 cycles
        latency = 10;
        DEBUG_PRINT("InvalidateSig latency: " << latency << " cycles");
    }
    
    return currentCycle + latency;
}

void Bus::notifyRequester(cycle_t currentCycle, const BusTransaction& transaction) {
    // Handle WriteBack case first
    if (transaction.type == BusRequestType::WriteBack) {
        // For WriteBack, just need to notify cache that writeback is complete
        DEBUG_PRINT("Cycle " << currentCycle << ": WriteBack completed for Core " 
                    << transaction.requesterId);
                  
        // For a writeback, we set the state to INVALID to indicate this was a writeback completion
        // This is a convention between the Bus and Cache classes - writebacks don't update any line's state
        // but we need to distinguish them from other transactions
        caches[transaction.requesterId]->notifyTransactionComplete(
            currentCycle, transaction.address, CacheLineState::INVALID);
        return;
    } else if (transaction.type == BusRequestType::InvalidateSig) {
        // For InvalidateSig, no need to allocate a block or update state
        // This is a write hit to a shared line, so the state should be modified
        DEBUG_PRINT("Cycle " << currentCycle << ": InvalidateSig completed for Core "
                    << transaction.requesterId);
                    
        // Set state to MODIFIED since this was a write hit to a shared line
        caches[transaction.requesterId]->notifyTransactionComplete(
            currentCycle, transaction.address, CacheLineState::MODIFIED);
        return;
    }
    
    // For read/write requests, determine the appropriate new state for the block
    CacheLineState newState;
    
    if (transaction.type == BusRequestType::BusRd) {
        // If another cache has the block, it goes to Shared
        // Otherwise, it goes to Exclusive
        newState = transaction.servedByCache ? 
            CacheLineState::SHARED : CacheLineState::EXCLUSIVE;
            
        DEBUG_PRINT("Cycle " << currentCycle << ": BusRd completed for Core " 
                    << transaction.requesterId << ", served by cache: " 
                    << (transaction.servedByCache ? "yes" : "no")
                    << ", new state: " << getCacheLineStateString(newState));
    } else if (transaction.type == BusRequestType::BusRdX) {
        // Always goes to Modified for BusRdX, regardless of whether another cache had it
        newState = CacheLineState::MODIFIED;
        
        DEBUG_PRINT("Cycle " << currentCycle << ": BusRdX completed for Core " 
                    << transaction.requesterId 
                    << ", new state: " << getCacheLineStateString(newState));
    } else {
        std::cerr << "ERROR: Invalid transaction type in notifyRequester: " 
                  << static_cast<int>(transaction.type) << std::endl;
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

// Helper for debugging
std::string Bus::getBusRequestTypeString(BusRequestType type) const {
    switch (type) {
        case BusRequestType::BusRd: return "BusRd";
        case BusRequestType::BusRdX: return "BusRdX";
        case BusRequestType::WriteBack: return "WriteBack";
        case BusRequestType::InvalidateSig: return "InvalidateSig";
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
