#ifndef REPLACEMENT_POLICY_H
#define REPLACEMENT_POLICY_H

#include <vector>
#include <string>
#include "CacheTypes.h"

class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() = default;

    // Returns the index of the victim in the set
    virtual size_t findVictim(const std::vector<CacheLine>& set) = 0;

    // Called on Cache Hits
    virtual void onAccess(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) = 0;

    // Called on Cache Misses (Insertion)
    virtual void onInsert(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) = 0;

    virtual std::string getName() const = 0;
};

class LRUPolicy : public ReplacementPolicy {
public:
    size_t findVictim(const std::vector<CacheLine>& set) override;
    void onAccess(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) override;
    void onInsert(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) override;
    std::string getName() const override { return "LRU"; }
};

class FIFOPolicy : public ReplacementPolicy {
public:
    size_t findVictim(const std::vector<CacheLine>& set) override;
    void onAccess(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) override;
    void onInsert(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) override;
    std::string getName() const override { return "FIFO"; }
};

class LFUPolicy : public ReplacementPolicy {
public:
    size_t findVictim(const std::vector<CacheLine>& set) override;
    void onAccess(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) override;
    void onInsert(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) override;
    std::string getName() const override { return "LFU"; }
};

class OptimalPolicy : public ReplacementPolicy {
public:
    size_t findVictim(const std::vector<CacheLine>& set) override;
    void onAccess(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) override;
    void onInsert(CacheLine& line, uint64_t currentTime, uint64_t nextUseStep) override;
    std::string getName() const override { return "Optimal"; }
};

#endif // REPLACEMENT_POLICY_H
