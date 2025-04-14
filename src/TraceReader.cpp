#include "TraceReader.h"
#include <iostream>
#include <cctype>
#include <sstream>

TraceReader::TraceReader(const std::string& filename) {
    fileStream.open(filename);
    if (!fileStream.is_open()) {
        std::cerr << "Error opening trace file: " << filename << std::endl;
        eof = true;
    }
}

TraceReader::~TraceReader() {
    if (fileStream.is_open()) {
        fileStream.close();
    }
}

bool TraceReader::getNextTrace(TraceEntry& entry) {
    if (eof || !fileStream.is_open()) {
        return false;
    }

    std::string line;
    if (!std::getline(fileStream, line)) {
        eof = true;
        return false;
    }

    // Parse the line
    std::istringstream iss(line);
    char opChar;
    std::string addrStr;
    
    if (!(iss >> opChar >> addrStr)) {
        std::cerr << "Error parsing trace line: " << line << std::endl;
        return false;
    }

    // Determine operation
    if (opChar == 'R' || opChar == 'r') {
        entry.op = MemOperation::READ;
    } else if (opChar == 'W' || opChar == 'w') {
        entry.op = MemOperation::WRITE;
    } else {
        std::cerr << "Unknown operation '" << opChar << "' in trace line: " << line << std::endl;
        return false;
    }

    // Parse hex address, handling 0x prefix if present
    if (addrStr.substr(0, 2) == "0x" || addrStr.substr(0, 2) == "0X") {
        addrStr = addrStr.substr(2);
    }

    // Pad the hex string to 8 digits (32 bits)
    while (addrStr.length() < 8) {
        addrStr = "0" + addrStr;
    }

    // Convert hex string to uint32_t
    try {
        entry.addr = std::stoul(addrStr, nullptr, 16);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing address '" << addrStr << "': " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool TraceReader::isEOF() const {
    return eof;
} 
