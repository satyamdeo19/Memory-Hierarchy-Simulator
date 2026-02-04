#include "MemoryHierarchy.h"
#include "ReplacementPolicy.h"
#include <iostream>

MemoryHierarchy::MemoryHierarchy() : blockSize(64) {}

// Helper for Policy Creation
std::unique_ptr<ReplacementPolicy> createPolicy(uint32_t id) {
    if (id == 1) return std::make_unique<FIFOPolicy>();
    if (id == 2) return std::make_unique<LFUPolicy>();
    if (id == 3) return std::make_unique<OptimalPolicy>();
    return std::make_unique<LRUPolicy>();
} 

void MemoryHierarchy::initialize(uint32_t l1Size, uint32_t l1Assoc, 
                               uint32_t l2Size, uint32_t l2Assoc, 
                               uint32_t l3Size, uint32_t l3Assoc, uint32_t l3Latency,
                               uint32_t ramSize, uint32_t ramAssoc, uint32_t diskLatency,
                               uint32_t tSize, uint32_t tLatency,
                               uint32_t bSize, uint32_t pSize,
                               uint32_t policyID) {
    blockSize = bSize;
    pageSize = pSize;
    if (pageSize == 0) pageSize = 4096; 

    l1Cache = std::make_unique<Cache>(l1Size, blockSize, l1Assoc, createPolicy(policyID));
    l2Cache = std::make_unique<Cache>(l2Size, blockSize, l2Assoc, createPolicy(policyID));
    l3Cache = std::make_unique<Cache>(l3Size, blockSize, l3Assoc, createPolicy(policyID));
    ramCache = std::make_unique<Cache>(ramSize, pageSize, ramAssoc, createPolicy(policyID));
    
    L3_LATENCY = l3Latency;
    DISK_LATENCY = diskLatency;
    seenBlocks.clear();
    
    // VM
    pageTable.clear();
    nextPPN = 0;
    
    // Initialize Free PPN List
    freePPNs.clear();
    uint32_t numFrames = ramSize / pageSize;
    if (numFrames == 0) numFrames = 1;

    for (uint32_t i = 0; i < numFrames; ++i) {
        freePPNs.push_back(i);
    }
    
    // TLB
    tlbSize = tSize;
    tlbLatency = tLatency;
    tlb.assign(tlbSize, {0, 0, 0, false});
    tlbAccessCounter = 0;
}

uint32_t MemoryHierarchy::reconstructAddress(uint32_t tag, uint32_t setIndex, const Cache& cache) {
    uint32_t numSets = cache.getNumSets();
    uint32_t bSize = cache.getBlockSize();
    
    uint32_t offsetBits = 0;
    while ((1u << offsetBits) < bSize) offsetBits++;
    
    uint32_t indexBits = 0;
    while ((1u << indexBits) < numSets) indexBits++;
    
    return (tag << (offsetBits + indexBits)) | (setIndex << offsetBits);
}

void MemoryHierarchy::invalidateTLB(uint32_t vpn) {
    for (auto& entry : tlb) {
        if (entry.valid && entry.vpn == vpn) {
            entry.valid = false;
        }
    }
}

AccessResult MemoryHierarchy::access(uint32_t address, AccessType type, uint64_t currentStep, uint64_t nextUseStep) {
    AccessResult result;
    result.l1Hit = false;
    result.l2Hit = false;
    result.l3Hit = false;
    result.ramHit = false;
    result.tlbHit = false;
    result.totalLatency = 0;
    result.events.l1MissType = MissType::None;
    result.events.l2MissType = MissType::None;
    result.events.l3MissType = MissType::None;
    result.events.ramMissType = MissType::None;
    
    result.virtualAddress = address;
    result.pageFault = false;
    result.tlbLatency = tlbLatency;
    result.totalLatency += tlbLatency;
    
    uint32_t vpn = address / pageSize;
    uint32_t offset = address % pageSize;
    result.vpn = vpn;

    uint32_t ppn;
    
    // 1. Check TLB
    int tlbIndex = -1;
    for (int i = 0; i < tlb.size(); ++i) {
        if (tlb[i].valid && tlb[i].vpn == vpn) {
             tlbIndex = i;
             break;
        }
    }
    
    if (tlbIndex != -1) {
         // TLB Hit
         result.tlbHit = true;
         ppn = tlb[tlbIndex].ppn;
         tlb[tlbIndex].lastAccess = ++tlbAccessCounter;
         
         ramCache->probe(vpn * pageSize, AccessType::READ, currentStep, nextUseStep);
         
    } else {
        // TLB Miss
        bool pageInRam = pageTable.find(vpn) != pageTable.end();
        
        if (pageInRam) {
            ppn = pageTable[vpn];
            ramCache->probe(vpn * pageSize, AccessType::READ, currentStep, nextUseStep);
            
        } else {
            // Page Fault
            result.pageFault = true;
            result.totalLatency += DISK_LATENCY;
            result.events.ramMissType = MissType::Cold; 
            
            EvictionInfo ramEvict = ramCache->insert(vpn * pageSize, currentStep, nextUseStep);
            
            if (ramEvict.occurred) {
                uint32_t victimPageAddr = reconstructAddress(ramEvict.tag, ramEvict.set, *ramCache);
                uint32_t victimVPN = victimPageAddr / pageSize;
                
                result.events.evictions.push_back({"RAM_PAGE", ramEvict});
                
                if (pageTable.count(victimVPN)) {
                    uint32_t vPPN = pageTable[victimVPN];
                    pageTable.erase(victimVPN);
                    invalidateTLB(victimVPN);
                    ppn = vPPN;
                } else {
                    ppn = 0; 
                }
                
                if (ramEvict.isDirty) {
                    result.totalLatency += DISK_LATENCY;
                }
                
            } else {
                if (!freePPNs.empty()) {
                    ppn = freePPNs.back();
                    freePPNs.pop_back();
                } else {
                    ppn = 0; 
                }
            }
            
            pageTable[vpn] = ppn;
        }
        
        // Update TLB (LRU)
        int lruIdx = 0;
        uint64_t minTime = 0xFFFFFFFFFFFFFFFF; 
        for (int i = 0; i < tlb.size(); ++i) {
            if (!tlb[i].valid) { lruIdx = i; break; }
            if (tlb[i].lastAccess < minTime) {
                minTime = tlb[i].lastAccess;
                lruIdx = i;
            }
        }
        tlb[lruIdx] = {vpn, ppn, ++tlbAccessCounter, true};
    }
    
    result.ppn = ppn;
    uint32_t physAddr = (ppn * pageSize) + offset;
    result.physicalAddress = physAddr;
    
    uint32_t blockAddr = physAddr / blockSize;
    bool isCold = seenBlocks.find(blockAddr) == seenBlocks.end();
    if (isCold) seenBlocks.insert(blockAddr);

    // 1. Check L1
    result.totalLatency += L1_LATENCY;
    if (l1Cache->probe(physAddr, type, currentStep, nextUseStep)) {
        result.l1Hit = true;
        return result;
    }
    
    // L1 Miss
    if (isCold) result.events.l1MissType = MissType::Cold;
    else if (l1Cache->isFull()) result.events.l1MissType = MissType::Capacity;
    else result.events.l1MissType = MissType::Conflict;

    // 2. Check L2
    result.totalLatency += L2_LATENCY;
    bool foundInL2 = l2Cache->probe(physAddr, AccessType::READ, currentStep, nextUseStep);
    
    if (foundInL2) {
        result.l2Hit = true;
    } else {
        if (isCold) result.events.l2MissType = MissType::Cold;
        else if (l2Cache->isFull()) result.events.l2MissType = MissType::Capacity;
        else result.events.l2MissType = MissType::Conflict;

        // 3. Check L3
        result.totalLatency += L3_LATENCY;
        bool foundInL3 = l3Cache->probe(physAddr, AccessType::READ, currentStep, nextUseStep);

        if (foundInL3) {
            result.l3Hit = true;
        } else {
            if (isCold) result.events.l3MissType = MissType::Cold;
            else if (l3Cache->isFull()) result.events.l3MissType = MissType::Capacity;
            else result.events.l3MissType = MissType::Conflict;

            // Fetch from RAM
            result.ramHit = true; 
            result.totalLatency += 100; // RAM Latency

            // Fill L3
            EvictionInfo l3Evict = l3Cache->insert(physAddr, currentStep, nextUseStep);
            if (l3Evict.occurred) {
                result.events.evictions.push_back({"L3", l3Evict});
            }
        }
        
        // Fill L2
        EvictionInfo l2Evict = l2Cache->insert(physAddr, currentStep, nextUseStep);
        if (l2Evict.occurred) {
            result.events.evictions.push_back({"L2", l2Evict});
            if (l2Evict.isDirty) {
                uint32_t victimAddrL2 = reconstructAddress(l2Evict.tag, l2Evict.set, *l2Cache);
                if (!l3Cache->probe(victimAddrL2, AccessType::WRITE, currentStep, nextUseStep)) {
                     EvictionInfo l3EvictWB = l3Cache->insert(victimAddrL2, currentStep, nextUseStep);
                     if (l3EvictWB.occurred) {
                         result.events.evictions.push_back({"L3_WB", l3EvictWB});
                     }
                     l3Cache->markDirty(victimAddrL2);
                }
            }
        }
    }

    // Fill L1
    EvictionInfo l1Evict = l1Cache->insert(physAddr, currentStep, nextUseStep);
    if (l1Evict.occurred) {
        result.events.evictions.push_back({"L1", l1Evict});
        if (l1Evict.isDirty) {
            uint32_t victimAddrL1 = reconstructAddress(l1Evict.tag, l1Evict.set, *l1Cache);
            if (!l2Cache->probe(victimAddrL1, AccessType::WRITE, currentStep, nextUseStep)) {
                 EvictionInfo l2EvictWB = l2Cache->insert(victimAddrL1, currentStep, nextUseStep);
                 if (l2EvictWB.occurred) {
                     result.events.evictions.push_back({"L2_WB", l2EvictWB});
                     if (l2EvictWB.isDirty) {
                         uint32_t victimAddrL2 = reconstructAddress(l2EvictWB.tag, l2EvictWB.set, *l2Cache);
                         if (!l3Cache->probe(victimAddrL2, AccessType::WRITE, currentStep, nextUseStep)) {
                              EvictionInfo l3EvictWB = l3Cache->insert(victimAddrL2, currentStep, nextUseStep);
                              if (l3EvictWB.occurred) {
                                   result.events.evictions.push_back({"L3_WB_Casc", l3EvictWB});
                              }
                              l3Cache->markDirty(victimAddrL2);
                         }
                     }
                     l2Cache->markDirty(victimAddrL1);
                 }
            }
        }
    }

    if (type == AccessType::WRITE) {
        l1Cache->markDirty(physAddr);
        ramCache->markDirty(vpn * pageSize);
    }

    return result;
}
