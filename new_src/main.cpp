#include <iostream>
#include <string>
#include <cstdlib>  // For exit()
#include <getopt.h> 
#include "Simulator.h"

void printUsage(const char* programName) {
    std::cerr << "Usage: " << programName << " -t <tracefile> [-s <s>] [-E <E>] [-b <b>] [-o <outfilename>] [-h]" << std::endl;
    std::cerr << "  -t <tracefile>: name of parallel application (e.g. app1) whose 4 traces are to be used" << std::endl;
    std::cerr << "  -s <s>: number of set index bits (number of sets in the cache = S = 2^s)" << std::endl;
    std::cerr << "  -E <E>: associativity (number of cache lines per set)" << std::endl;
    std::cerr << "  -b <b>: number of block bits (block size = B = 2^b)" << std::endl;
    std::cerr << "  -o <outfilename>: logs output in file for plotting etc." << std::endl;
    std::cerr << "  -h: prints this help" << std::endl;
}

int main(int argc, char* argv[]) {
    // Default parameters
    std::string tracePrefix = "";
    int indexBits = 6;       // 64 sets by default
    int associativity = 2;   // 2-way set associative by default
    int blockOffsetBits = 5; // 32 byte block size by default
    std::string outputFile = ""; // Default to stdout
    
    // Parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "t:s:E:b:o:h")) != -1) {
        switch (opt) {
            case 't':
                tracePrefix = optarg;
                break;
            case 's':
                indexBits = std::atoi(optarg);
                break;
            case 'E':
                associativity = std::atoi(optarg);
                break;
            case 'b':
                blockOffsetBits = std::atoi(optarg);
                break;
            case 'o':
                outputFile = optarg;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }
    
    // Validate required parameters
    if (tracePrefix.empty()) {
        std::cerr << "Error: trace file prefix (-t) is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Redirect output if specified
    std::ofstream outFileStream;
    std::streambuf* originalCoutBuffer = nullptr;
    
    if (!outputFile.empty()) {
        outFileStream.open(outputFile);
        if (!outFileStream) {
            std::cerr << "Error: Could not open output file: " << outputFile << std::endl;
            return 1;
        }
        // Save original cout buffer and redirect cout to file
        originalCoutBuffer = std::cout.rdbuf();
        std::cout.rdbuf(outFileStream.rdbuf());
    }
    
    // Create and run simulator
    Simulator sim(tracePrefix, indexBits, associativity, blockOffsetBits);
    sim.run();
    sim.printStats();
    
    // Restore cout if it was redirected
    if (originalCoutBuffer) {
        std::cout.rdbuf(originalCoutBuffer);
        outFileStream.close();
    }
    
    return 0;
}