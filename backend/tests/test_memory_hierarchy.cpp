// test_memory_hierarchy.cpp — Integration tests for MemoryHierarchy
// Uses doctest (vendored single header).

#define DOCTEST_CONFIG_NO_MULTITHREADING  // MinGW 6.3: no std::mutex/thread
#include "doctest.h"
#include "MemoryHierarchy.h"

// ─── Helper: create a standard test hierarchy ─────────────────────────────────
static MemoryHierarchy makeHierarchy(uint32_t policyID = 0 /*LRU*/) {
    MemoryHierarchy mem;
    mem.initialize(
        /*l1Size*/   512, /*l1Assoc*/ 2,
        /*l2Size*/  4096, /*l2Assoc*/ 4,
        /*l3Size*/  8192, /*l3Assoc*/ 8, /*l3Lat*/ 20,
        /*ramSize*/ 65536, /*ramAssoc*/ 16, /*diskLat*/ 10000,
        /*tlbSize*/ 4,   /*tlbLat*/ 1,
        /*blkSz*/ 64,    /*pgSz*/ 4096,
        policyID);
    return mem;
}

// ─── Tests ───────────────────────────────────────────────────────────────────
TEST_CASE("L1HitOnSecondAccess") {
    auto mem = makeHierarchy();
    mem.access(0x1000, AccessType::READ, 0, UINT64_MAX);
    AccessResult res = mem.access(0x1000, AccessType::READ, 1, UINT64_MAX);
    CHECK(res.l1Hit);
    CHECK_FALSE(res.l2Hit);
}

TEST_CASE("FirstAccessIsAlwaysMiss") {
    auto mem = makeHierarchy();
    AccessResult res = mem.access(0x2000, AccessType::READ, 0, UINT64_MAX);
    CHECK_FALSE(res.l1Hit);
}

TEST_CASE("TLBHitOnSecondPageAccess") {
    auto mem = makeHierarchy();
    mem.access(0x3000, AccessType::READ, 0, UINT64_MAX);   // TLB miss (page fault)
    AccessResult res = mem.access(0x3010, AccessType::READ, 1, UINT64_MAX); // same page
    CHECK(res.tlbHit);
}

TEST_CASE("TLBMissOnFirstPageAccess") {
    auto mem = makeHierarchy();
    AccessResult res = mem.access(0x5000, AccessType::READ, 0, UINT64_MAX);
    CHECK_FALSE(res.tlbHit);
}

TEST_CASE("PageFaultOnFirstPageAccess") {
    auto mem = makeHierarchy();
    AccessResult res = mem.access(0xA000, AccessType::READ, 0, UINT64_MAX);
    CHECK(res.pageFault);
}

TEST_CASE("NoPageFaultOnRepeatPageAccess") {
    auto mem = makeHierarchy();
    mem.access(0xB000, AccessType::READ, 0, UINT64_MAX);
    AccessResult res = mem.access(0xB000, AccessType::READ, 1, UINT64_MAX);
    CHECK_FALSE(res.pageFault);
}

TEST_CASE("VPNComputedCorrectly") {
    auto mem = makeHierarchy();
    // pageSize=4096; address 0x7ABC → VPN = 0x7ABC / 0x1000 = 7
    AccessResult res = mem.access(0x7ABC, AccessType::READ, 0, UINT64_MAX);
    CHECK(res.vpn == 0x7u);
}

TEST_CASE("PhysicalAddressContainsPageOffset") {
    auto mem = makeHierarchy();
    // offset = 0x7ABC % 0x1000 = 0xABC
    AccessResult res = mem.access(0x7ABC, AccessType::READ, 0, UINT64_MAX);
    CHECK((res.physicalAddress % 4096) == 0xABCu);
}

TEST_CASE("WriteHitIncursL1Hit") {
    auto mem = makeHierarchy();
    mem.access(0xC000, AccessType::READ, 0, UINT64_MAX);  // load into L1
    AccessResult res = mem.access(0xC000, AccessType::WRITE, 1, UINT64_MAX);
    CHECK(res.l1Hit);
}

TEST_CASE("AllPoliciesRunWithoutError") {
    for (uint32_t pol = 0; pol < 4; ++pol) {
        auto mem = makeHierarchy(pol);
        // Should not throw
        mem.access(0x1000, AccessType::READ,  0, UINT64_MAX);
        mem.access(0x2000, AccessType::WRITE, 1, UINT64_MAX);
        mem.access(0x1000, AccessType::READ,  2, UINT64_MAX);
    }
}
