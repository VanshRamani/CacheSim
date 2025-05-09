
#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <vector>
#include <string>
#include "Types.h"
#include "Core.h"
#include "Cache.h"
#include "Bus.h"

// Simulator class to manage the overall simulation
class Simulator {
private:
    // Simulation state
    cycle_t currentCycle;
    
    // Components
    Bus bus;
    std::vector<Core> cores;
    std::vector<Cache> caches;
    
    // Configuration
    std::string traceBaseName;
    int numCores;
    int indexBits;        // s
    int associativity;    // E
    int blockOffsetBits;  // b
    int blockSize;        // B = 2^b
    int numSets;          // S = 2^s
    int cacheSize;        // Size in bytes = S * E * B
    
    // Debug flag
    static bool debugEnabled;
    
    // Helper methods
    void initialize();
    void tick();
    bool checkFinished();

public:
    Simulator(const std::string& traceBase, int s, int E, int b);
    
    // Run the simulation
    void run();
    
    // Print statistics
    void printStats(const std::string& outfile = "");
    
    // Get simulation parameters
    int getIndexBits() const;
    int getAssociativity() const;
    int getBlockOffsetBits() const;
    int getBlockSize() const;
    int getNumSets() const;
    int getCacheSize() const;
    
    // Get number of cycles executed
    cycle_t getCurrentCycle() const;
    
    // Debug control
    static void setDebugEnabled(bool enabled);
    static bool isDebugEnabled();
};

// Macro for debug output
#define DEBUG_PRINT(x) if (Simulator::isDebugEnabled()) { std::cout << "DEBUG: " << x << std::endl; }

#endif // SIMULATOR_H 
