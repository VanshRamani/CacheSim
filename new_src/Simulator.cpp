#include "Simulator.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>

Simulator::Simulator(const std::string& traceBase, int s, int E, int b) :
    currentCycle(0),
    bus(b), 
    traceBaseName(traceBase),
    numCores(4),
    indexBits(s),
    associativity(E),
    blockOffsetBits(b) {
    
    // Calculate derived parameters
    blockSize = 1 << blockOffsetBits;
    numSets = 1 << indexBits;
    cacheSize = numSets * associativity * blockSize;
    
    // Allocate space for cores and caches
    cores.reserve(numCores);
    caches.reserve(numCores);
}

void Simulator::initialize() {
    // Create caches
    for (int i = 0; i < numCores; i++) {
        caches.emplace_back(i, indexBits, associativity, blockOffsetBits, &bus);
        bus.addCache(&caches.back());
    }
    
    // Create cores with their trace files
    for (int i = 0; i < numCores; i++) {
        std::string tracePath = traceBaseName + "_proc" + std::to_string(i) + ".trace";
        cores.emplace_back(i, &caches[i], tracePath);
    }
}

void Simulator::run() {
    // Initialize the simulation components
    initialize();
    
    // Make sure currentCycle starts at 0
    currentCycle = 0;
    
    // Run until all cores are finished
    while (!checkFinished()) {
        tick();
    }
    
    // Update total cycles for all cores
    for (Core& core : cores) {
        core.setTotalCycles(currentCycle);
    }
}

void Simulator::tick() {
    // Process bus transactions first
    bus.tick(currentCycle);
    
    // Process core operations
    for (auto& core : cores) {
        core.tick(currentCycle);
    }
    
    // Increment cycle counter
    currentCycle++;
}

bool Simulator::checkFinished() {
    for (const Core& core : cores) {
        if (!core.isFinished()) {
            return false;
        }
    }
    return true;
}

void Simulator::printStats(const std::string& outfile) {
    // Prepare output stream
    std::ostream* out = &std::cout;
    std::ofstream fileStream;
    
    if (!outfile.empty()) {
        fileStream.open(outfile);
        if (fileStream.is_open()) {
            out = &fileStream;
        } else {
            std::cerr << "Error opening output file: " << outfile << std::endl;
        }
    }
    
    // Print simulation parameters
    *out << "Simulation Parameters:" << std::endl;
    *out << "Trace Prefix: " << traceBaseName << std::endl;
    *out << "Set Index Bits: " << indexBits << std::endl;
    *out << "Associativity: " << associativity << std::endl;
    *out << "Block Bits: " << blockOffsetBits << std::endl;
    *out << "Block Size (Bytes): " << blockSize << std::endl;
    *out << "Number of Sets: " << numSets << std::endl;
    *out << "Cache Size (KB per core): " << (cacheSize / 1024.0) << std::endl;
    *out << "MESI Protocol: Enabled" << std::endl;
    *out << "Write Policy: Write-back, Write-allocate" << std::endl;
    *out << "Replacement Policy: LRU" << std::endl;
    *out << "Bus: Central snooping bus" << std::endl;
    *out << std::endl;
    
    // Print per-core statistics
    for (int i = 0; i < numCores; i++) {
        const Core& core = cores[i];
        const Cache& cache = caches[i];
        
        *out << "Core " << i << " Statistics:" << std::endl;
        *out << "Total Instructions: " << core.getInstructionCount() << std::endl;
        *out << "Total Reads: " << core.getReadCount() << std::endl;
        *out << "Total Writes: " << core.getWriteCount() << std::endl;
        *out << "Total Execution Cycles: " << core.getTotalCycles() << std::endl;
        *out << "Idle Cycles: " << core.getIdleCycles() << std::endl;
        *out << "Cache Misses: " << cache.getMisses() << std::endl;
        *out << "Cache Miss Rate: " << std::fixed << std::setprecision(2) 
             << (cache.getMissRate() * 100.0) << "%" << std::endl;
        *out << "Cache Evictions: " << cache.getEvictions() << std::endl;
        *out << "Writebacks: " << cache.getWritebacks() << std::endl;
        *out << "Bus Invalidations: " << cache.getInvalidationsReceived() << std::endl;
        *out << "Data Traffic (Bytes): " << bus.getTotalDataTrafficBytes() << std::endl;
        *out << std::endl;
    }
    
    // Print overall bus summary
    *out << "Overall Bus Summary:" << std::endl;
    *out << "Total Bus Transactions: " << bus.getTotalBusTransactions() << std::endl;
    *out << "Total Bus Traffic (Bytes): " << bus.getTotalDataTrafficBytes() << std::endl;
    
    // Close file if opened
    if (fileStream.is_open()) {
        fileStream.close();
    }
}

// Getters
int Simulator::getIndexBits() const {
    return indexBits;
}

int Simulator::getAssociativity() const {
    return associativity;
}

int Simulator::getBlockOffsetBits() const {
    return blockOffsetBits;
}

int Simulator::getBlockSize() const {
    return blockSize;
}

int Simulator::getNumSets() const {
    return numSets;
}

int Simulator::getCacheSize() const {
    return cacheSize;
}

cycle_t Simulator::getCurrentCycle() const {
    return currentCycle;
} 
