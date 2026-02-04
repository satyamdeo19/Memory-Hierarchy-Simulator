#include "MemoryAddress.h"
#include <cmath>
#include <iostream>

MemoryAddress::MemoryAddress(uint32_t address, uint32_t blockSize, uint32_t numSets)
    : address(address), blockSize(blockSize), numSets(numSets) {
    
    // Validate powers of 2 (simple check)
    // In a production environment, we'd want more robust validation
    if (blockSize == 0 || (blockSize & (blockSize - 1)) != 0) {
        throw std::invalid_argument("Block size must be a power of 2");
    }
    if (numSets == 0 || (numSets & (numSets - 1)) != 0) {
        throw std::invalid_argument("Number of sets must be a power of 2");
    }

    // Compute bit widths
    offsetBits = (uint32_t)std::log2(blockSize);
    indexBits = (uint32_t)std::log2(numSets);
    tagBits = 32 - offsetBits - indexBits;

    decode();
}

void MemoryAddress::decode() {
    // START: Mask creation logic
    // Offset mask: bits 0 to offsetBits-1
    uint32_t offsetMask = (1 << offsetBits) - 1;
    offset = address & offsetMask;

    // Index mask: bits offsetBits to offsetBits+indexBits-1
    // Shift right by offsetBits, then mask with (1 << indexBits) - 1
    uint32_t indexMask = (1 << indexBits) - 1;
    index = (address >> offsetBits) & indexMask;

    // Tag: remaining bits
    // Shift right by (offsetBits + indexBits)
    tag = address >> (offsetBits + indexBits);
    // END: Mask creation logic
}

uint32_t MemoryAddress::getTag() const {
    return tag;
}

uint32_t MemoryAddress::getIndex() const {
    return index;
}

uint32_t MemoryAddress::getOffset() const {
    return offset;
}
