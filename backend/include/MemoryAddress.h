#ifndef MEMORY_ADDRESS_H
#define MEMORY_ADDRESS_H

#include <cstdint>
#include <cmath>
#include <stdexcept>

class MemoryAddress {
public:
    MemoryAddress(uint32_t address, uint32_t blockSize, uint32_t numSets);

    uint32_t getTag() const;
    uint32_t getIndex() const;
    uint32_t getOffset() const;
    
    // Getters for bit counts (useful for debugging/UI)
    uint32_t getOffsetBits() const { return offsetBits; }
    uint32_t getIndexBits() const { return indexBits; }
    uint32_t getTagBits() const { return tagBits; }

private:
    uint32_t address;
    uint32_t blockSize;
    uint32_t numSets;

    uint32_t offsetBits;
    uint32_t indexBits;
    uint32_t tagBits;

    uint32_t tag;
    uint32_t index;
    uint32_t offset;

    void decode();
};

#endif // MEMORY_ADDRESS_H
