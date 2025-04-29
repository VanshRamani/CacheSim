#include "Bus.h"
#include "Cache.h"
#include <iostream>
#include <algorithm>

// In Bus.cpp:
Bus::Bus(int blockSize) :
    caches(),
    requestQueue(),
    busy(false),
    busyUntilCycle(0),
    roundRobinArbiter(0),
    blockSizeBytes(blockSize),
    totalDataTrafficBytes(0),
    totalBusTransactions(0) {
    // Constructor body
}

void Bus::addCache(Cache* cache) {
    caches.push_back(cache);
}

void Bus::pushRequest(int requesterId, BusRequestType type, address_t address, cycle_t currentCycle) {
    BusTransaction transaction;
    transaction.requesterId = requesterId;
    transaction.type = type;
    transaction.address = address;
    transaction.startCycle = currentCycle;
    transaction.completionCycle = 0;
    transaction.dataReady = false;
    transaction.servedByCache = false;
    transaction.dataAvailableInMemory = false;  // Initialize the new field

    requestQueue.push_back(transaction);
}

size_t Bus::getQueueSize() const {
    return requestQueue.size();
}

void Bus::tick(cycle_t currentCycle) {
    if (busy) {
        if (currentCycle >= busyUntilCycle) {
            busy = false;
            notifyRequester(currentCycle, currentTransaction);
        }
        return;
    }

    if (requestQueue.empty()) return;

    // Start processing the next request
    currentTransaction = requestQueue.front();
    requestQueue.erase(requestQueue.begin());

    totalBusTransactions++;

    bool suppliedByCache = broadcastSnoop(currentCycle, currentTransaction);

    cycle_t completionCycle = calculateCompletionTime(currentCycle, currentTransaction, suppliedByCache);
    currentTransaction.completionCycle = completionCycle;
    currentTransaction.servedByCache = suppliedByCache;

    busy = true;
    busyUntilCycle = completionCycle;

    totalDataTrafficBytes += blockSizeBytes;
}

// In Bus.cpp - handleWriteback method
void Bus::handleWriteback(cycle_t currentCycle, address_t address, int sourceId) {
    // Find if this writeback is in response to an outstanding request
    for (auto& req : requestQueue) {
        if (req.address == address) {
            // Mark that data is now in memory for this request
            req.dataAvailableInMemory = true;
        }
    }
    
    // Update stats
    totalDataTrafficBytes += blockSizeBytes;
    totalBusTransactions++;
}


bool Bus::broadcastSnoop(cycle_t currentCycle, const BusTransaction& transaction) {
    bool suppliedByCache = false;
    for (Cache* cache : caches) {
        if (cache->getId() != transaction.requesterId) {
            cache->handleBusRequest(currentCycle, transaction.type, transaction.address);
            //check if any cache has the data.
            if(transaction.type == BusRequestType::BusRd || transaction.type == BusRequestType::BusRdX){
                CacheLine* line = cache->findBlock(transaction.address);
                if(line != nullptr && (line->getState() == CacheLineState::MODIFIED || line->getState() == CacheLineState::EXCLUSIVE)){
                    suppliedByCache = true;
                }
            }
        }
    }
    return suppliedByCache;
}

cycle_t Bus::calculateCompletionTime(cycle_t currentCycle, const BusTransaction& transaction, bool suppliedByCache) {
     if (transaction.type == BusRequestType::BusRd || transaction.type == BusRequestType::BusRdX) {
        if (suppliedByCache) {
            return currentCycle + busLatency; // Cache-to-cache transfer
        }
        else {
            return currentCycle + memoryLatency; // Memory access
        }
    }
    else if(transaction.type == BusRequestType::WriteBack || transaction.type == BusRequestType::Flush){
        return currentCycle + memoryLatency;
    }
    else {
        return currentCycle + busLatency;
    }
}
// In Bus.cpp - completeTransaction method
void Bus::completeTransaction(cycle_t currentCycle) {
    // First check if any cache has the block in M state
    Cache* ownerCache = nullptr;
    bool modifiedInOtherCache = false;
    
    for (size_t i = 0; i < caches.size(); i++) {
        if (static_cast<int>(i) != currentTransaction.requesterId) {
            CacheLine* line = caches[i]->findBlock(currentTransaction.address);
            if (line != nullptr && line->getState() == CacheLineState::MODIFIED) {
                ownerCache = caches[i];
                modifiedInOtherCache = true;
                break;
            }
        }
    }
    
    if (modifiedInOtherCache && currentTransaction.type == BusRequestType::BusRdX) {
        // Another cache has it in M state - it must write back first
        // The owner will see this in its snoop and do the writeback
        
        // After writeback completes, requester gets block in M state
        bool suppliedByCache = broadcastSnoop(currentCycle, currentTransaction);
        
        // Calculate when transaction will complete
        currentTransaction.completionCycle = calculateCompletionTime(
            currentCycle, currentTransaction, suppliedByCache);
        currentTransaction.servedByCache = suppliedByCache;
    } else {
        // Standard transaction handling
        // ...rest of your existing code...
    }
}

void Bus::notifyRequester(cycle_t currentCycle, const BusTransaction& transaction) {
    CacheLineState newState;
    if (transaction.type == BusRequestType::BusRd) {
        newState = transaction.servedByCache ? CacheLineState::SHARED : CacheLineState::EXCLUSIVE;
    }
    else if (transaction.type == BusRequestType::BusRdX) {
        newState = CacheLineState::MODIFIED;
    }
    else if (transaction.type == BusRequestType::WriteBack || transaction.type == BusRequestType::Flush) {
        return;
    }
    caches[transaction.requesterId]->notifyTransactionComplete(currentCycle, transaction.address, newState);
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
