#ifndef CACHE_TYPES_H
#define CACHE_TYPES_H

#include <cstdint>
#include <string>

enum class AccessType {
    READ,
    WRITE
};

struct CacheLine {
    bool valid;
    bool dirty; // Added dirty bit
    uint32_t tag;
    
    uint64_t lastAccessTime;
    uint64_t insertionTime;
    uint32_t frequency;
    uint64_t nextUseStep; // For Optimal Policy

    CacheLine() 
        : valid(false), dirty(false), tag(0), lastAccessTime(0), insertionTime(0), frequency(0), nextUseStep(0xFFFFFFFFFFFFFFFF) {}
};

struct EvictionInfo {
    bool occurred;
    bool isDirty; 
    uint32_t set;
    uint32_t tag;
    std::string policy;
    int64_t nextUseDistance; // Metadata for Optimal
    
    EvictionInfo() : occurred(false), isDirty(false), set(0), tag(0), policy(""), nextUseDistance(-1) {}
};

enum class MissType {
    None,
    Cold,
    Conflict,
    Capacity
};

#endif // CACHE_TYPES_H
