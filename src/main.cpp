#include <iostream>
#include <string>
#include <unistd.h>
#include <getopt.h>
#include <filesystem>
#include "Simulator.h"

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " -t <tracefile> -s <s> -E <E> -b <b> [-o <outfile>] [-d] [-h]" << std::endl;
    std::cout << "-t <tracefile>: Name of the trace file (without the _proc{0,1,2,3}.trace suffix)" << std::endl;
    std::cout << "-s <s>: Number of set index bits (number of sets = 2^s)" << std::endl;
    std::cout << "-E <E>: Associativity (number of lines per set)" << std::endl;
    std::cout << "-b <b>: Number of block bits (block size = 2^b bytes)" << std::endl;
    std::cout << "-o <outfile>: Output file for statistics (default: stdout)" << std::endl;
    std::cout << "-d, --debug: Enable debug output" << std::endl;
    std::cout << "-h, --help: Print this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    // Default parameters
    std::string tracePrefix = "";
    int s = 4;          // Index bits: default 4 -> 16 sets
    int E = 4;          // Associativity: default 4-way
    int b = 6;          // Block offset bits: default 6 -> 64 byte blocks
    std::string outfile = "";
    bool debug = false;  // Debug output: default disabled
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-t") {
            if (i + 1 < argc) {
                tracePrefix = argv[++i];
            } else {
                std::cerr << "Error: -t requires a trace name argument" << std::endl;
                return 1;
            }
        } else if (arg == "-s") {
            if (i + 1 < argc) {
                s = std::stoi(argv[++i]);
            } else {
                std::cerr << "Error: -s requires a set index bits argument" << std::endl;
                return 1;
            }
        } else if (arg == "-E") {
            if (i + 1 < argc) {
                E = std::stoi(argv[++i]);
            } else {
                std::cerr << "Error: -E requires an associativity argument" << std::endl;
                return 1;
            }
        } else if (arg == "-b") {
            if (i + 1 < argc) {
                b = std::stoi(argv[++i]);
            } else {
                std::cerr << "Error: -b requires a block bits argument" << std::endl;
                return 1;
            }
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                outfile = argv[++i];
            } else {
                std::cerr << "Error: -o requires an output file name argument" << std::endl;
                return 1;
            }
        } else if (arg == "-d" || arg == "--debug") {
            debug = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Check required arguments
    if (tracePrefix.empty()) {
        std::cerr << "Error: Trace file name (-t) is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Validate parameters
    if (s <= 0 || E <= 0 || b <= 0) {
        std::cerr << "Error: s, E, and b must be positive integers." << std::endl;
        return 1;
    }
    
    try {
        // Set debug mode
        Simulator::setDebugEnabled(debug);
        
        if (debug) {
            std::cout << "Debug mode enabled" << std::endl;
        }
        
        // Create simulator with the full trace path
        Simulator simulator(tracePrefix, s, E, b);
        
        // Run simulation
        simulator.run();
        
        // Print statistics
        simulator.printStats(outfile);
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

void printHelp() {
    std::cout << "Usage: ./L1simulate -t <tracefile> -s <s> -E <E> -b <b> [-o <outfile>] [-d] [-h]" << std::endl;
    std::cout << "-t <tracefile>: Name of the trace file (without the _proc{0,1,2,3}.trace suffix)" << std::endl;
    std::cout << "-s <s>: Number of set index bits (number of sets = 2^s)" << std::endl;
    std::cout << "-E <E>: Associativity (number of lines per set)" << std::endl;
    std::cout << "-b <b>: Number of block bits (block size = 2^b bytes)" << std::endl;
    std::cout << "-o <outfile>: Output file for statistics (default: stdout)" << std::endl;
    std::cout << "-d, --debug: Enable debug output" << std::endl;
    std::cout << "-h, --help: Print this help message" << std::endl;
} 
