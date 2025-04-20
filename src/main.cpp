// Aditya Anand, 2023CS50284
// Vansh Ramani, 2023CS50804

#include <iostream>
#include <string>
#include <unistd.h>
#include <getopt.h>
#include "Simulator.h"

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  -t <tracefile>: name of parallel application (e.g. app1) whose 4 traces are to be used\n";
    std::cout << "  -s <s>: number of set index bits (number of sets in the cache = S = 2^s)\n";
    std::cout << "  -E <E>: associativity (number of cache lines per set)\n";
    std::cout << "  -b <b>: number of block bits (block size = B = 2^b)\n";
    std::cout << "  -o <outfilename>: logs output in file for plotting etc.\n";
    std::cout << "  -h: prints this help\n";
}

int main(int argc, char* argv[]) {
    // Default parameters
    std::string traceFile = "";
    int s = 6;  // 64 sets by default
    int E = 2;  // 2-way associative by default
    int b = 5;  // 32-byte blocks by default
    std::string outFile = "";
    
    // Parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "t:s:E:b:o:h")) != -1) {
        switch (opt) {
            case 't':
                traceFile = optarg;
                break;
            case 's':
                s = std::stoi(optarg);
                break;
            case 'E':
                E = std::stoi(optarg);
                break;
            case 'b':
                b = std::stoi(optarg);
                break;
            case 'o':
                outFile = optarg;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                std::cerr << "Unknown option: " << static_cast<char>(opt) << std::endl;
                printUsage(argv[0]);
                return 1;
        }
    }
    
    // Check required parameters
    if (traceFile.empty()) {
        std::cerr << "Error: Trace file is required." << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Validate parameters
    if (s <= 0 || E <= 0 || b <= 0) {
        std::cerr << "Error: s, E, and b must be positive integers." << std::endl;
        return 1;
    }
    
    try {
        // Create simulator
        Simulator simulator(traceFile, s, E, b);
        
        // Run simulation
        simulator.run();
        
        // Print statistics
        simulator.printStats(outFile);
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 
