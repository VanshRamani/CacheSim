#ifndef TRACEREADER_H
#define TRACEREADER_H

#include <fstream>
#include <string>
#include "Types.h"

class TraceReader {
private:
    std::ifstream fileStream;
    bool eof = false;

public:
    // Constructor takes filename of trace to read
    TraceReader(const std::string& filename);
    
    // Destructor to close file handle
    ~TraceReader();
    
    // Read the next trace entry
    // Returns true if successful, false if EOF
    bool getNextTrace(TraceEntry& entry);
    
    // Check if we've reached end of file
    bool isEOF() const;
};

#endif // TRACEREADER_H 