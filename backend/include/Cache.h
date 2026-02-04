#ifndef CACHE_H
#define CACHE_H

#include "MemoryAddress.h"
#include "CacheTypes.h"
#include "ReplacementPolicy.h"
#include <vector>
#include <memory>
#include <cstdint>


class Cache {
public:
    Cache(uint32_t cacheSize, uint32_t blockSize, uint32_t associativity, std::unique_ptr<ReplacementPolicy> policy);

    // Checks for tag match. If isWrite is true and Hit, sets dirty bit.
    bool probe(uint32_t rawAddress, AccessType type, uint64_t currentStep, uint64_t nextUseStep);

    // Installs block (initially clean). Returns eviction info.
    EvictionInfo insert(uint32_t rawAddress, uint64_t currentStep, uint64_t nextUseStep);

    // Explicitly marks a block as dirty (useful for write hits after fill)
    void markDirty(uint32_t rawAddress);

    bool isFull() const;

    uint32_t getNumSets() const { return numSets; }
    uint32_t getAssociativity() const { return associativity; }
    uint32_t getBlockSize() const { return blockSize; }

private:
    uint32_t cacheSize;
    uint32_t blockSize;
    uint32_t associativity;
    uint32_t numSets;
    
    uint64_t accessCounter; 

    std::vector<std::vector<CacheLine>> sets;
    std::unique_ptr<ReplacementPolicy> policy;
};

#endif // CACHE_H
