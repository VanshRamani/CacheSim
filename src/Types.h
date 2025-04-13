#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <string>

typedef uint32_t address_t;
typedef uint64_t cycle_t;

// MESI protocol states
enum class CacheLineState { 
    MODIFIED,    // Line has been modified, only this cache has a valid copy
    EXCLUSIVE,   // Line is unmodified, only this cache has a valid copy
    SHARED,      // Line is unmodified, may exist in other caches
    INVALID      // Line is invalid, contains no useful data
};

// Bus request types
enum class BusRequestType { 
    BusRd,       // Read request, issued on read miss
    BusRdX,      // Read exclusive request, issued on write miss or write to shared line
    WriteBack,   // Writeback request, issued when evicting a modified line
    None         // No request
};

// Memory operation types
enum class MemOperation { 
    READ,
    WRITE 
};

// Structure to represent a trace entry
struct TraceEntry { 
    MemOperation op; 
    address_t addr; 
};

#endif // TYPES_H 