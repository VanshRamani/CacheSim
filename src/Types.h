#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <string>

typedef uint32_t address_t;
typedef uint64_t cycle_t;

// MESI protocol states
enum class CacheLineState { 
    MODIFIED = 0b00,    // Line has been modified, only this cache has a valid copy
    EXCLUSIVE = 0b01,   // Line is unmodified, only this cache has a valid copy
    SHARED = 0b10,      // Line is unmodified, may exist in other caches
    INVALID = 0b11     // Line is invalid, contains no useful data
};

enum class BusRequestPriority {
    LOW,    // For prefetch requests
    NORMAL  // For regular requests
};

// Bus request types
enum class BusRequestType { 
    BusRd = 2,       // Read request, issued on read miss
    BusRdX = 3,      // Read exclusive request, issued on write miss or write to shared line
    WriteBack = 1,   // Writeback request, issued when evicting a modified line
    None = 0         // No request
};

// Memory operation types
enum class MemOperation { 
    READ,
    WRITE 
};

// Structure to represent a bus transaction
struct BusTransaction {
    int requesterId;                // ID of the requesting cache
    BusRequestType type;           // Type of bus request
    address_t address;             // Target address
    cycle_t startCycle;           // When the request was initiated
    cycle_t completionCycle;      // When the request will complete
    bool dataReady;               // Whether data is ready for requester
    bool servedByCache;           // Whether another cache provided the data
    BusRequestPriority priority;  // Priority level of the request
};

// Structure to represent a trace entry
struct TraceEntry { 
    MemOperation op; 
    address_t addr; 
};

#endif // TYPES_H 
