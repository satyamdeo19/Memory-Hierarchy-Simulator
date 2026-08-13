#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define DOCTEST_CONFIG_NO_MULTITHREADING  // MinGW 6.3: no std::mutex/thread
// MinGW 6.3 doesn't have gmtime_s (MSVC-only); provide a compat shim BEFORE doctest.h
#if defined(__MINGW32__)
#include <time.h>
#ifndef gmtime_s
inline static int _mingw_gmtime_s(struct tm* tm_out, const time_t* t) {
    struct tm* r = gmtime(t);   // not thread-safe, but tests are single-threaded
    if (!r) return 1;
    *tm_out = *r;
    return 0;
}
#define gmtime_s(tm, t) _mingw_gmtime_s((tm), (t))
#endif
#endif
#include "doctest.h"
// test_cache.cpp — Unit tests for the Cache class
// Tests: cold miss, hit detection, eviction, dirty bit, markDirty, isFull.

#include "Cache.h"
#include "ReplacementPolicy.h"

// ─── Helpers ────────────────────────────────────────────────────────────────
// Direct-mapped: 4 sets, 1-way, 16-byte blocks (total = 64 bytes)
static Cache makeDM() {
    return Cache(64, 16, 1, std::make_unique<LRUPolicy>());
}
// 2-way set-associative: 2 sets, 2-way, 16-byte blocks (total = 64 bytes)
static Cache make2Way() {
    return Cache(64, 16, 2, std::make_unique<LRUPolicy>());
}

// ─── Tests ──────────────────────────────────────────────────────────────────
TEST_CASE("ColdMissOnFirstAccess") {
    Cache cache = makeDM();
    CHECK_FALSE(cache.probe(0x0000, AccessType::READ, 0, UINT64_MAX));
}

TEST_CASE("HitAfterInsert") {
    Cache cache = makeDM();
    cache.insert(0x0000, 0, UINT64_MAX);
    CHECK(cache.probe(0x0000, AccessType::READ, 1, UINT64_MAX));
}

TEST_CASE("DifferentBlockIsMiss") {
    Cache cache = makeDM();
    cache.insert(0x0000, 0, UINT64_MAX);
    // 0x0100 maps to a different set (set 1 with numSets=4, blockSize=16)
    CHECK_FALSE(cache.probe(0x0100, AccessType::READ, 1, UINT64_MAX));
}

TEST_CASE("EvictionOccursWhenFull") {
    // Direct-mapped: set 0 holds one block; second block with same set index evicts it
    Cache cache = makeDM();
    // addr 0x00 → set 0; addr 0x40 → set 0 (0x40/16 % 4 = 4 % 4 = 0)
    cache.insert(0x0000, 0, UINT64_MAX);
    EvictionInfo info = cache.insert(0x0040, 1, UINT64_MAX);
    CHECK(info.occurred);
}

TEST_CASE("NoEvictionWithEmptyWay") {
    // 2-way: numSets=2, blockSize=16
    // 0x00 → set 0 (tag=0); 0x20 → set 0 (tag=1) — second way is empty, no eviction
    Cache cache = make2Way();
    cache.insert(0x0000, 0, UINT64_MAX);
    EvictionInfo info = cache.insert(0x0020, 1, UINT64_MAX);
    CHECK_FALSE(info.occurred);
}

TEST_CASE("DirtyBitSetOnWriteHit") {
    Cache cache = makeDM();
    cache.insert(0x0000, 0, UINT64_MAX);
    CHECK(cache.probe(0x0000, AccessType::WRITE, 1, UINT64_MAX)); // write → hit
    // Evict the dirty block
    EvictionInfo info = cache.insert(0x0040, 2, UINT64_MAX);
    CHECK(info.occurred);
    CHECK(info.isDirty);
}

TEST_CASE("CleanEvictionReportsNotDirty") {
    Cache cache = makeDM();
    cache.insert(0x0000, 0, UINT64_MAX);  // read (clean)
    EvictionInfo info = cache.insert(0x0040, 1, UINT64_MAX);
    CHECK(info.occurred);
    CHECK_FALSE(info.isDirty);
}

TEST_CASE("MarkDirtyWorks") {
    Cache cache = makeDM();
    cache.insert(0x0000, 0, UINT64_MAX);
    cache.markDirty(0x0000);
    EvictionInfo info = cache.insert(0x0040, 1, UINT64_MAX);
    CHECK(info.isDirty);
}

TEST_CASE("IsFullDetection") {
    Cache cache = makeDM(); // 4 sets
    CHECK_FALSE(cache.isFull());
    cache.insert(0x00, 0, UINT64_MAX); // set 0
    cache.insert(0x10, 0, UINT64_MAX); // set 1
    cache.insert(0x20, 0, UINT64_MAX); // set 2
    cache.insert(0x30, 0, UINT64_MAX); // set 3
    CHECK(cache.isFull());
}
