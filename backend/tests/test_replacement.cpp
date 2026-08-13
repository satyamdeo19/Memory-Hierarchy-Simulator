// test_replacement.cpp — Unit tests for all four replacement policies
// Uses doctest (vendored single header).

#define DOCTEST_CONFIG_NO_MULTITHREADING  // MinGW 6.3: no std::mutex/thread
#include "doctest.h"
#include "ReplacementPolicy.h"
#include "CacheTypes.h"
#include <limits>

// ─── Helper: build a valid CacheLine ─────────────────────────────────────────
static CacheLine makeLine(uint64_t lastAccess, uint64_t insertTime,
                          uint32_t freq, uint64_t nextUse, uint32_t tag = 0) {
    CacheLine l;
    l.valid          = true;
    l.dirty          = false;
    l.tag            = tag;
    l.lastAccessTime = lastAccess;
    l.insertionTime  = insertTime;
    l.frequency      = freq;
    l.nextUseStep    = nextUse;
    return l;
}

// ─── LRU ────────────────────────────────────────────────────────────────────
TEST_CASE("LRU_EvictsLeastRecentlyUsed") {
    LRUPolicy pol;
    std::vector<CacheLine> set = {
        makeLine(5,  0, 1, UINT64_MAX), // oldest → victim
        makeLine(10, 0, 1, UINT64_MAX),
        makeLine(15, 0, 1, UINT64_MAX), // most recent
    };
    CHECK(pol.findVictim(set) == 0u);
}

TEST_CASE("LRU_EmptySlotReturnedFirst") {
    LRUPolicy pol;
    std::vector<CacheLine> set(3);
    set[0].valid = true;  set[0].lastAccessTime = 100;
    set[1].valid = false; // empty
    set[2].valid = true;  set[2].lastAccessTime = 50;
    CHECK(pol.findVictim(set) == 1u);
}

TEST_CASE("LRU_OnAccessUpdatesTimestamp") {
    LRUPolicy pol;
    CacheLine line = makeLine(1, 1, 1, UINT64_MAX);
    pol.onAccess(line, 42, UINT64_MAX);
    CHECK(line.lastAccessTime == 42u);
}

// ─── FIFO ────────────────────────────────────────────────────────────────────
TEST_CASE("FIFO_EvictsFirstInserted") {
    FIFOPolicy pol;
    std::vector<CacheLine> set = {
        makeLine(10, 1, 1, UINT64_MAX), // inserted first (insertionTime=1) → victim
        makeLine(1,  5, 1, UINT64_MAX),
        makeLine(2, 10, 1, UINT64_MAX),
    };
    CHECK(pol.findVictim(set) == 0u);
}

TEST_CASE("FIFO_OnAccessDoesNotChangeInsertionTime") {
    FIFOPolicy pol;
    CacheLine line = makeLine(1, 3, 1, UINT64_MAX);
    uint64_t before = line.insertionTime;
    pol.onAccess(line, 99, UINT64_MAX);
    CHECK(line.insertionTime == before);
}

TEST_CASE("FIFO_OnInsertSetsInsertionTime") {
    FIFOPolicy pol;
    CacheLine line;
    pol.onInsert(line, 7, UINT64_MAX);
    CHECK(line.insertionTime == 7u);
}

// ─── LFU ────────────────────────────────────────────────────────────────────
TEST_CASE("LFU_EvictsLowestFrequency") {
    LFUPolicy pol;
    std::vector<CacheLine> set = {
        makeLine(10, 0, 3, UINT64_MAX),
        makeLine(10, 0, 1, UINT64_MAX), // lowest freq → victim
        makeLine(10, 0, 5, UINT64_MAX),
    };
    CHECK(pol.findVictim(set) == 1u);
}

TEST_CASE("LFU_TieBreaksByRecency") {
    LFUPolicy pol;
    std::vector<CacheLine> set = {
        makeLine(5,  0, 2, UINT64_MAX), // older → victim in tie
        makeLine(10, 0, 2, UINT64_MAX), // same freq, more recent
    };
    CHECK(pol.findVictim(set) == 0u);
}

TEST_CASE("LFU_OnAccessIncrementsFrequency") {
    LFUPolicy pol;
    CacheLine line = makeLine(1, 1, 3, UINT64_MAX);
    pol.onAccess(line, 5, UINT64_MAX);
    CHECK(line.frequency == 4u);
}

TEST_CASE("LFU_OnInsertSetsFrequencyToOne") {
    LFUPolicy pol;
    CacheLine line;
    pol.onInsert(line, 1, UINT64_MAX);
    CHECK(line.frequency == 1u);
}

// ─── Optimal (Belady's) ──────────────────────────────────────────────────────
TEST_CASE("Optimal_EvictsFarthestNextUse") {
    OptimalPolicy pol;
    std::vector<CacheLine> set = {
        makeLine(0, 0, 1, 5),   // next use at step 5
        makeLine(0, 0, 1, 20),  // farthest → victim
        makeLine(0, 0, 1, 10),
    };
    CHECK(pol.findVictim(set) == 1u);
}

TEST_CASE("Optimal_NeverUsedBlockIsVictim") {
    OptimalPolicy pol;
    const uint64_t INF = std::numeric_limits<uint64_t>::max();
    std::vector<CacheLine> set = {
        makeLine(0, 0, 1, 3),
        makeLine(0, 0, 1, INF), // never used → victim
        makeLine(0, 0, 1, 7),
    };
    CHECK(pol.findVictim(set) == 1u);
}

TEST_CASE("Optimal_OnAccessUpdatesNextUse") {
    OptimalPolicy pol;
    CacheLine line = makeLine(0, 0, 1, 999);
    pol.onAccess(line, 5, 42);
    CHECK(line.nextUseStep == 42u);
}
