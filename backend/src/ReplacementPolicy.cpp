#include "ReplacementPolicy.h"
#include <limits>

// --- LRU Policy ---

size_t LRUPolicy::findVictim(const std::vector<CacheLine>& set) {
    size_t victimIndex = 0;
    uint64_t minTime = std::numeric_limits<uint64_t>::max();

    for (size_t i = 0; i < set.size(); ++i) {
        if (!set[i].valid) return i;
        if (set[i].lastAccessTime < minTime) {
            minTime = set[i].lastAccessTime;
            victimIndex = i;
        }
    }
    return victimIndex;
}

void LRUPolicy::onAccess(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) {
    line.lastAccessTime = currentTime;
    line.nextUseStep = nextUseStep; // Track Oracle info
}

void LRUPolicy::onInsert(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) {
    line.lastAccessTime = currentTime;
    line.nextUseStep = nextUseStep; // Track Oracle info
}


// --- FIFO Policy ---

size_t FIFOPolicy::findVictim(const std::vector<CacheLine>& set) {
    size_t victimIndex = 0;
    uint64_t minTime = std::numeric_limits<uint64_t>::max();

    for (size_t i = 0; i < set.size(); ++i) {
        if (!set[i].valid) return i;
        if (set[i].insertionTime < minTime) {
            minTime = set[i].insertionTime;
            victimIndex = i;
        }
    }
    return victimIndex;
}

void FIFOPolicy::onAccess(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) {
    line.nextUseStep = nextUseStep; // Track Oracle info even if FIFO doesn't update recency
}

void FIFOPolicy::onInsert(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) {
    line.insertionTime = currentTime;
    line.nextUseStep = nextUseStep; // Track Oracle info
}

// --- LFU Policy ---

size_t LFUPolicy::findVictim(const std::vector<CacheLine>& set) {
    size_t victimIndex = 0;
    uint32_t minFreq = std::numeric_limits<uint32_t>::max();
    uint64_t minTime = std::numeric_limits<uint64_t>::max();

    for (size_t i = 0; i < set.size(); ++i) {
        if (!set[i].valid) return i;
        
        if (set[i].frequency < minFreq) {
            minFreq = set[i].frequency;
            minTime = set[i].lastAccessTime;
            victimIndex = i;
        } else if (set[i].frequency == minFreq) {
            if (set[i].lastAccessTime < minTime) {
                minTime = set[i].lastAccessTime;
                victimIndex = i;
            }
        }
    }
    return victimIndex;
}

void LFUPolicy::onAccess(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) {
    line.lastAccessTime = currentTime;
    line.frequency++;
    line.nextUseStep = nextUseStep; // Track Oracle info
}

void LFUPolicy::onInsert(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) {
    line.lastAccessTime = currentTime;
    line.insertionTime = currentTime;
    line.frequency = 1;
    line.nextUseStep = nextUseStep; // Track Oracle info
}

// --- Optimal Policy ---

size_t OptimalPolicy::findVictim(const std::vector<CacheLine>& set) {
    size_t victimIndex = 0;
    uint64_t maxNextUse = 0;

    for (size_t i = 0; i < set.size(); ++i) {
        if (!set[i].valid) return i;
        
        if (set[i].nextUseStep > maxNextUse) {
            maxNextUse = set[i].nextUseStep;
            victimIndex = i;
        }
    }
    return victimIndex;
}

void OptimalPolicy::onAccess(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) {
    line.lastAccessTime = currentTime;
    line.nextUseStep = nextUseStep;
}

void OptimalPolicy::onInsert(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) {
    line.lastAccessTime = currentTime;
    line.insertionTime = currentTime;
    line.nextUseStep = nextUseStep;
}
