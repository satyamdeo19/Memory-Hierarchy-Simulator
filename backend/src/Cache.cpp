#include "Cache.h"
#include <iostream>
#include <stdexcept>
#include <iomanip>

Cache::Cache(uint32_t cacheSize, uint32_t blockSize, uint32_t associativity, std::unique_ptr<ReplacementPolicy> policy)
    : cacheSize(cacheSize), blockSize(blockSize), associativity(associativity), policy(std::move(policy)), accessCounter(0) {
    if (blockSize == 0 || associativity == 0) throw std::invalid_argument("Invalid config");
    uint32_t totalBlockCapacity = cacheSize / blockSize;
    numSets = totalBlockCapacity / associativity;
    if (numSets == 0) throw std::invalid_argument("Cache size too small");
    sets.resize(numSets);
    for (auto& set : sets) set.resize(associativity);
}

bool Cache::isFull() const {
    for (const auto& set : sets) {
        for (const auto& line : set) {
            if (!line.valid) return false;
        }
    }
    return true;
}

bool Cache::probe(uint32_t rawAddress, AccessType type, uint64_t currentStep, uint64_t nextUseStep) {
    MemoryAddress addr(rawAddress, blockSize, numSets);
    // Using provided currentStep instead of internal accessCounter
    uint32_t index = addr.getIndex();
    uint32_t tag = addr.getTag();

    if (index >= numSets) return false; 
    auto& set = sets[index];

    for (auto& line : set) {
        if (line.valid && line.tag == tag) {
            policy->onAccess(line, currentStep, nextUseStep);
            if (type == AccessType::WRITE) {
                line.dirty = true;
            }
            return true;
        }
    }
    return false;
}

void Cache::markDirty(uint32_t rawAddress) {
    MemoryAddress addr(rawAddress, blockSize, numSets);
    uint32_t index = addr.getIndex();
    uint32_t tag = addr.getTag();
    if (index >= numSets) return;
    
    for (auto& line : sets[index]) {
        if (line.valid && line.tag == tag) {
            line.dirty = true;
            return;
        }
    }
}

EvictionInfo Cache::insert(uint32_t rawAddress, uint64_t currentStep, uint64_t nextUseStep) {
    EvictionInfo info;
    info.occurred = false;

    MemoryAddress addr(rawAddress, blockSize, numSets);
    uint32_t index = addr.getIndex();
    uint32_t tag = addr.getTag();

    if (index >= numSets) return info;
    auto& set = sets[index];

    // Check avail (safe guard)
    for (auto& line : set) {
        if (line.valid && line.tag == tag) {
            policy->onInsert(line, currentStep, nextUseStep); 
            return info;
        }
    }

    int emptySlot = -1;
    for (size_t i = 0; i < set.size(); ++i) {
        if (!set[i].valid) {
            emptySlot = i;
            break;
        }
    }

    if (emptySlot != -1) {
        CacheLine& line = set[emptySlot];
        line.valid = true;
        line.tag = tag;
        line.dirty = false; 
        policy->onInsert(line, currentStep, nextUseStep);
    } else {
        size_t victimIdx = policy->findVictim(set);
        CacheLine& victim = set[victimIdx];
        
        info.occurred = true;
        info.isDirty = victim.dirty;
        info.set = index;
        info.tag = victim.tag;
        info.policy = policy->getName();
        
        // Metadata for Optimal Policy verification
        if (victim.nextUseStep == 0xFFFFFFFFFFFFFFFF) {
            info.nextUseDistance = -1;
        } else {
            // Check for negative? Should not happen if logic correct (Next Use > Current)
            if (victim.nextUseStep > currentStep)
                info.nextUseDistance = (int64_t)(victim.nextUseStep - currentStep);
            else
                info.nextUseDistance = 0; // Should be impossible unless bug
        }

        victim.tag = tag;
        victim.valid = true;
        victim.dirty = false;
        policy->onInsert(victim, currentStep, nextUseStep);
    }
    return info;
}
