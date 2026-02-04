#ifndef MEMORY_HIERARCHY_H
#define MEMORY_HIERARCHY_H

#include "Cache.h"
#include <memory> 
#include <cstdint>
#include <vector>
#include <unordered_set>
#include <unordered_map>

struct SimulationEvent {
    std::vector<std::pair<std::string, EvictionInfo>> evictions; 
    MissType l1MissType;
    MissType l2MissType;
    MissType l3MissType;
    MissType ramMissType;
};

struct TLBEntry {
    uint32_t vpn;
    uint32_t ppn;
    uint64_t lastAccess;
    bool valid;
};

struct AccessResult {
    bool l1Hit;
    bool l2Hit;
    bool l3Hit;
    bool ramHit;
    bool tlbHit; 
    uint32_t tlbLatency;
    uint32_t totalLatency;
    
    // VM Info
    bool pageFault;
    uint32_t virtualAddress;
    uint32_t physicalAddress;
    uint32_t vpn;
    uint32_t ppn;
    
    SimulationEvent events;
};

class MemoryHierarchy {
public:
    MemoryHierarchy();
    
    void initialize(
        uint32_t l1Size, uint32_t l1Assoc, 
        uint32_t l2Size, uint32_t l2Assoc, 
        uint32_t l3Size, uint32_t l3Assoc, uint32_t l3Latency,
        uint32_t ramSize, uint32_t ramAssoc, uint32_t diskLatency,
        uint32_t tlbSize, uint32_t tlbLatency,
        uint32_t blockSize, uint32_t pageSize,
        uint32_t policyID // 0=LRU, 1=FIFO, 2=LFU
    );

    AccessResult access(uint32_t address, AccessType type, uint64_t currentStep, uint64_t nextUseStep);

private:
    std::unique_ptr<Cache> l1Cache;
    std::unique_ptr<Cache> l2Cache;
    std::unique_ptr<Cache> l3Cache;
    std::unique_ptr<Cache> ramCache;
    
    uint32_t blockSize; 
    std::unordered_set<uint32_t> seenBlocks; 

    // VM
    uint32_t pageSize;
    std::unordered_map<uint32_t, uint32_t> pageTable; // VPN -> PPN
    uint32_t nextPPN;
    std::vector<uint32_t> freePPNs; // List of free Physical Page Numbers
    
    void invalidateTLB(uint32_t vpn);

    // TLB
    std::vector<TLBEntry> tlb;
    uint32_t tlbSize;
    uint32_t tlbLatency;
    uint64_t tlbAccessCounter;

    // Latencies
    uint32_t L1_LATENCY = 1;
    uint32_t L2_LATENCY = 10;
    uint32_t L3_LATENCY = 20;
    uint32_t RAM_LATENCY = 100;
    uint32_t DISK_LATENCY = 10000;
    
    // Helper to resolve address from tag+set
    uint32_t reconstructAddress(uint32_t tag, uint32_t setIndex, const Cache& cache);
};

#endif // MEMORY_HIERARCHY_H
