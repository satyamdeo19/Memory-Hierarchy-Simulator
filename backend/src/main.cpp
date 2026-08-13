#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <map>
#ifndef __EMSCRIPTEN__
#include <windows.h>   // CreateThread / WaitForMultipleObjects
#endif
#include "MemoryHierarchy.h"

// ═══════════════════════════════════════════════════════════════════════════
//  Helper Types
// ═══════════════════════════════════════════════════════════════════════════

std::string missTypeToString(MissType t) {
    switch (t) {
        case MissType::Cold:     return "COLD";
        case MissType::Conflict: return "CONFLICT";
        case MissType::Capacity: return "CAPACITY";
        default:                 return "NONE";
    }
}

struct LevelStats {
    uint32_t hits   = 0;
    uint32_t misses = 0;
    std::map<std::string, uint32_t> missTypes;
    void addMiss(MissType type) { misses++; missTypes[missTypeToString(type)]++; }
    void addHit()  { hits++; }
};

struct SimulationStats {
    LevelStats l1, l2, l3, ram, disk;
    uint64_t   totalLatency = 0;
    std::map<std::string, uint32_t> latencyHistogram;

    void record(const AccessResult& res) {
        if (res.l1Hit) { l1.addHit(); }
        else {
            l1.addMiss(res.events.l1MissType);
            if (res.l2Hit) { l2.addHit(); }
            else {
                l2.addMiss(res.events.l2MissType);
                if (res.l3Hit) { l3.addHit(); }
                else {
                    l3.addMiss(res.events.l3MissType);
                    if (res.ramHit) { ram.addHit(); }
                    else {
                        ram.addMiss(res.events.ramMissType);
                        if (res.pageFault) disk.addHit();
                    }
                }
            }
        }
        totalLatency += res.totalLatency;

        std::string bucket;
        if      (res.totalLatency <=    1) bucket = "1";
        else if (res.totalLatency <=   10) bucket = "2-10";
        else if (res.totalLatency <=   50) bucket = "11-50";
        else if (res.totalLatency <=  200) bucket = "51-200";
        else if (res.totalLatency <= 1000) bucket = "201-1000";
        else                               bucket = ">1000";
        latencyHistogram[bucket]++;
    }
};

// Per-access record (trace entry)
struct AccessRecord {
    std::string typeStr;
    uint32_t    val;
    AccessType  type;
    uint64_t    nextUse;
};

// All configuration parameters bundled together
struct SimulationConfig {
    uint32_t l1Size, l1Assoc;
    uint32_t l2Size, l2Assoc;
    uint32_t l3Size, l3Assoc, l3Latency;
    uint32_t ramSize, ramAssoc, diskLatency;
    uint32_t tlbSize, tlbLatency;
    uint32_t blockSize, pageSize;
    uint32_t policyID; // used only in single-simulation mode
};

// ═══════════════════════════════════════════════════════════════════════════
//  JSON Helpers
// ═══════════════════════════════════════════════════════════════════════════

// Build the "summary" JSON object as a std::string (indent = leading spaces)
std::string summaryToJSON(const SimulationStats& stats, const std::string& indent = "  ") {
    std::ostringstream ss;
    const std::string i2 = indent + "  ";

    auto levelJSON = [&](const std::string& name, const LevelStats& s) -> std::string {
        std::ostringstream ls;
        ls << i2 << "\"" << name << "\": {\n";
        ls << i2 << "  \"hits\": "   << s.hits   << ",\n";
        ls << i2 << "  \"misses\": " << s.misses  << ",\n";
        ls << i2 << "  \"miss_types\": {";
        int cnt = 0;
        if (!s.missTypes.empty()) {
            ls << "\n";
            for (auto const& p : s.missTypes) {
                ls << i2 << "    \"" << p.first << "\": " << p.second;
                if (++cnt < (int)s.missTypes.size()) ls << ",";
                ls << "\n";
            }
            ls << i2 << "  }";
        } else {
            ls << "}";
        }
        ls << "\n" << i2 << "}";
        return ls.str();
    };

    ss << indent << "{\n";
    ss << levelJSON("L1",   stats.l1)   << ",\n";
    ss << levelJSON("L2",   stats.l2)   << ",\n";
    ss << levelJSON("L3",   stats.l3)   << ",\n";
    ss << levelJSON("RAM",  stats.ram)  << ",\n";
    ss << levelJSON("DISK", stats.disk) << ",\n";
    ss << i2 << "\"total_latency\": " << stats.totalLatency << ",\n";
    ss << i2 << "\"latency_histogram\": {";
    int cnt = 0;
    if (!stats.latencyHistogram.empty()) {
        ss << "\n";
        for (auto const& p : stats.latencyHistogram) {
            ss << i2 << "  \"" << p.first << "\": " << p.second;
            if (++cnt < (int)stats.latencyHistogram.size()) ss << ",";
            ss << "\n";
        }
        ss << i2 << "}";
    } else {
        ss << "}";
    }
    ss << "\n" << indent << "}";
    return ss.str();
}

// Build the per-access JSON entry (original logic, unchanged)
std::string formatAccessJSON(uint32_t addrVal,
                             AccessType type,
                             const MemoryAddress& l1Addr,
                             const AccessResult& result) {
    std::stringstream ss;
    ss << "    {\n";
    ss << "      \"address\": \"0x"         << std::hex << addrVal                     << "\",\n";
    ss << "      \"virtual_address\": \"0x" << std::hex << result.virtualAddress       << "\",\n";
    ss << "      \"type\": \""               << (type == AccessType::READ ? "READ" : "WRITE") << "\",\n";
    ss << "      \"page_fault\": "           << (result.pageFault  ? "true" : "false") << ",\n";
    ss << "      \"tlb_hit\": "              << (result.tlbHit     ? "true" : "false") << ",\n";
    ss << "      \"tlb_latency\": "          << std::dec << result.tlbLatency           << ",\n";
    ss << "      \"physical_address\": \"0x"<< std::hex << result.physicalAddress      << "\",\n";
    ss << "      \"vpn\": \"0x"             << std::hex << result.vpn                  << "\",\n";
    ss << "      \"ppn\": \"0x"             << std::hex << result.ppn                  << "\",\n";

    ss << "      \"decomposition\": {\n";
    ss << "        \"tag\": \"0x"    << std::hex << l1Addr.getTag()    << "\",\n";
    ss << "        \"index\": \"0x"  << std::hex << l1Addr.getIndex()  << "\",\n";
    ss << "        \"offset\": \"0x" << std::hex << l1Addr.getOffset() << "\"\n";
    ss << "      },\n";

    ss << "      \"results\": [\n";
    // L1
    ss << "        {\n";
    ss << "          \"level\": \"L1\",\n";
    ss << "          \"hit\": "         << (result.l1Hit ? "true" : "false")         << ",\n";
    ss << "          \"latency\": "     << (result.l1Hit ? 1 : 0)                    << ",\n";
    ss << "          \"miss_type\": \"" << missTypeToString(result.events.l1MissType) << "\"\n";
    ss << "        },\n";
    // L2
    ss << "        {\n";
    ss << "          \"level\": \"L2\",\n";
    ss << "          \"hit\": "         << (result.l2Hit ? "true" : "false")         << ",\n";
    ss << "          \"latency\": 0,\n";
    ss << "          \"miss_type\": \"" << missTypeToString(result.events.l2MissType) << "\"\n";
    ss << "        },\n";
    // L3
    ss << "        {\n";
    ss << "          \"level\": \"L3\",\n";
    ss << "          \"hit\": "         << (result.l3Hit ? "true" : "false")         << ",\n";
    ss << "          \"latency\": 0,\n";
    ss << "          \"miss_type\": \"" << missTypeToString(result.events.l3MissType) << "\"\n";
    ss << "        },\n";
    // RAM
    ss << "        {\n";
    ss << "          \"level\": \"RAM\",\n";
    ss << "          \"hit\": "         << (result.ramHit ? "true" : "false")         << ",\n";
    ss << "          \"latency\": 0,\n";
    ss << "          \"miss_type\": \"" << missTypeToString(result.events.ramMissType) << "\"\n";
    ss << "        },\n";
    // DISK
    ss << "        {\n";
    ss << "          \"level\": \"DISK\",\n";
    ss << "          \"hit\": "     << (result.pageFault ? "true" : "false") << ",\n";
    ss << "          \"latency\": 0\n";
    ss << "        }\n";
    ss << "      ],\n";

    // Evictions
    ss << "      \"evictions\": [\n";
    for (size_t i = 0; i < result.events.evictions.size(); ++i) {
        auto& ev = result.events.evictions[i];
        ss << "        {\n";
        ss << "          \"level\": \""   << ev.first              << "\",\n";
        ss << "          \"set\": \"0x"   << std::hex << ev.second.set  << "\",\n";
        ss << "          \"tag\": \"0x"   << std::hex << ev.second.tag  << "\",\n";
        ss << "          \"policy\": \""  << ev.second.policy       << "\",\n";
        ss << "          \"dirty\": "     << (ev.second.isDirty ? "true" : "false") << ",\n";
        if (ev.second.nextUseDistance != -1)
            ss << "          \"next_use_distance\": " << std::dec << ev.second.nextUseDistance << "\n";
        else
            ss << "          \"next_use_distance\": \"inf\"\n";
        ss << "        }" << (i == result.events.evictions.size() - 1 ? "" : ",") << "\n";
    }
    ss << "      ],\n";
    ss << "      \"total_latency\": " << std::dec << result.totalLatency << "\n";
    ss << "    }";
    return ss.str();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Core Simulation Helpers
// ═══════════════════════════════════════════════════════════════════════════

// Read config from stdin — called once; fills cfg.policyID too for single mode.
bool readConfig(SimulationConfig& cfg, std::istream& in) {
    return !!(in
        >> cfg.l1Size >> cfg.l1Assoc
        >> cfg.l2Size >> cfg.l2Assoc
        >> cfg.l3Size >> cfg.l3Assoc >> cfg.l3Latency
        >> cfg.ramSize >> cfg.ramAssoc >> cfg.diskLatency
        >> cfg.tlbSize >> cfg.tlbLatency
        >> cfg.blockSize >> cfg.pageSize
        >> cfg.policyID);
}

// Read + preprocess the trace (computes nextUse via backward pass).
std::vector<AccessRecord> readTrace(std::istream& in) {
    size_t n;
    if (!(in >> n)) return {};

    std::vector<AccessRecord> trace;
    trace.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        std::string lineType;
        uint32_t addrVal;
        
        // Traces are usually hex. Use std::hex to parse addrVal correctly.
        if (!(in >> lineType >> std::hex >> addrVal >> std::dec)) break;
        AccessType t = (lineType == "W" || lineType == "w") ? AccessType::WRITE : AccessType::READ;
        trace.push_back({lineType, addrVal, t, 0xFFFFFFFFFFFFFFFF});
    }

    // Backward pass: compute next-use distance for Optimal policy
    std::unordered_map<uint32_t, uint64_t> lastSeen;
    for (int64_t i = (int64_t)trace.size() - 1; i >= 0; --i) {
        uint32_t addr = trace[i].val;
        if (lastSeen.count(addr))
            trace[i].nextUse = lastSeen[addr];
        lastSeen[addr] = (uint64_t)i;
    }
    return trace;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Thread-Safe Simulation Runner
//  Each invocation is fully independent — no shared mutable state.
// ═══════════════════════════════════════════════════════════════════════════
SimulationStats runPolicySimulation(const SimulationConfig& cfg,
                                    const std::vector<AccessRecord>& trace,
                                    uint32_t policyID) {
    MemoryHierarchy memory;
    memory.initialize(
        cfg.l1Size, cfg.l1Assoc,
        cfg.l2Size, cfg.l2Assoc,
        cfg.l3Size, cfg.l3Assoc, cfg.l3Latency,
        cfg.ramSize, cfg.ramAssoc, cfg.diskLatency,
        cfg.tlbSize, cfg.tlbLatency,
        cfg.blockSize, cfg.pageSize,
        policyID);

    SimulationStats stats;
    for (size_t i = 0; i < trace.size(); ++i) {
        AccessResult res = memory.access(trace[i].val, trace[i].type,
                                         (uint64_t)i, trace[i].nextUse);
        stats.record(res);
    }
    return stats;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Compare-All Mode  (--compare-all flag)
//  Runs all 4 replacement policies in parallel using std::thread.
//  Each thread owns its own MemoryHierarchy → zero shared mutable state.
// ═══════════════════════════════════════════════════════════════════════════
struct PolicyDef { uint32_t id; const char* name; };
constexpr PolicyDef kPolicies[4] = {
    {0, "LRU"}, {1, "FIFO"}, {2, "LFU"}, {3, "Optimal"}
};

// Windows thread payload — one per policy
struct PolicyTask {
    const SimulationConfig*          cfg;
    const std::vector<AccessRecord>* trace;
    uint32_t                         policyID;
    SimulationStats                  result;  // written exclusively by its thread
};

#ifndef __EMSCRIPTEN__
DWORD WINAPI policyThreadFunc(LPVOID param) {
    PolicyTask* task = static_cast<PolicyTask*>(param);
    task->result = runPolicySimulation(*task->cfg, *task->trace, task->policyID);
    return 0;
}
#endif

void runCompareAll(const SimulationConfig& cfg, const std::vector<AccessRecord>& trace, std::ostream& out) {
    PolicyTask tasks[4];
#ifndef __EMSCRIPTEN__
    HANDLE     handles[4];
#endif

    for (int i = 0; i < 4; ++i) {
        tasks[i].cfg      = &cfg;
        tasks[i].trace    = &trace;
        tasks[i].policyID = kPolicies[i].id;
#ifndef __EMSCRIPTEN__
        handles[i] = CreateThread(nullptr, 0, policyThreadFunc, &tasks[i], 0, nullptr);
        if (!handles[i]) {
            // Fallback: run sequentially if CreateThread fails
            tasks[i].result = runPolicySimulation(cfg, trace, kPolicies[i].id);
        }
#else
        // In WebAssembly, always run sequentially since pthreads require special headers
        tasks[i].result = runPolicySimulation(cfg, trace, kPolicies[i].id);
#endif
    }

#ifndef __EMSCRIPTEN__
    // Wait for all threads (only for handles that were successfully created)
    DWORD validCount = 0;
    HANDLE validHandles[4];
    for (int i = 0; i < 4; ++i) {
        if (handles[i]) validHandles[validCount++] = handles[i];
    }
    if (validCount > 0)
        WaitForMultipleObjects(validCount, validHandles, TRUE, INFINITE);
    for (int i = 0; i < 4; ++i)
        if (handles[i]) CloseHandle(handles[i]);
#endif

    // Emit JSON — single-threaded from here, no output races
    out << "{\n";
    out << "  \"mode\": \"compare\",\n";
    out << "  \"results\": [\n";
    for (int i = 0; i < 4; ++i) {
        out << "    {\n";
        out << "      \"policy\": \"" << kPolicies[i].name << "\",\n";
        out << "      \"summary\": " << summaryToJSON(tasks[i].result, "      ") << "\n";
        out << "    }";
        if (i < 3) out << ",";
        out << "\n";
    }
    out << "  ]\n}" << std::endl;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Single-Simulation Mode  (default, no flags)
//  Unchanged logic: outputs { trace: [...], summary: {...} }
// ═══════════════════════════════════════════════════════════════════════════
void runSingleSimulation(const SimulationConfig& cfg, const std::vector<AccessRecord>& trace, std::ostream& out) {
    MemoryHierarchy memory;
    memory.initialize(
        cfg.l1Size, cfg.l1Assoc,
        cfg.l2Size, cfg.l2Assoc,
        cfg.l3Size, cfg.l3Assoc, cfg.l3Latency,
        cfg.ramSize, cfg.ramAssoc, cfg.diskLatency,
        cfg.tlbSize, cfg.tlbLatency,
        cfg.blockSize, cfg.pageSize,
        cfg.policyID);

    uint32_t l1NumSets = cfg.l1Size / (cfg.blockSize * cfg.l1Assoc);

    std::vector<std::string> jsonObjects;
    jsonObjects.reserve(trace.size());
    SimulationStats stats;

    for (size_t i = 0; i < trace.size(); ++i) {
        AccessResult res = memory.access(trace[i].val, trace[i].type,
                                          (uint64_t)i, trace[i].nextUse);
        stats.record(res);

        uint32_t decompAddr = res.pageFault ? 0 : res.physicalAddress;
        MemoryAddress l1Addr(decompAddr, cfg.blockSize, l1NumSets);
        jsonObjects.push_back(formatAccessJSON(trace[i].val, trace[i].type, l1Addr, res));
    }

    out << "{\n";
    out << "  \"trace\": [\n";
    for (size_t i = 0; i < jsonObjects.size(); ++i) {
        out << jsonObjects[i];
        if (i < jsonObjects.size() - 1) out << ",";
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"summary\": " << summaryToJSON(stats, "  ") << "\n";
    out << "}" << std::endl;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Entry Point
// ═══════════════════════════════════════════════════════════════════════════
#ifndef __EMSCRIPTEN__
int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    bool compareAll = (argc > 1 && std::string(argv[1]) == "--compare-all");

    try {
        if (std::cin.peek() == EOF) {
            std::cout << "{ \"trace\": [], \"summary\": {} }" << std::endl;
            return 0;
        }

        SimulationConfig cfg{};
        if (!readConfig(cfg, std::cin)) {
            std::cout << "{ \"trace\": [], \"summary\": {} }" << std::endl;
            return 0;
        }

        auto trace = readTrace(std::cin);
        if (trace.empty()) {
            std::cout << "{ \"trace\": [], \"summary\": {} }" << std::endl;
            return 0;
        }

        if (compareAll) {
            runCompareAll(cfg, trace, std::cout);
        } else {
            runSingleSimulation(cfg, trace, std::cout);
        }

    } catch (const std::exception& e) {
        std::cerr << "Simulation Error: " << e.what() << std::endl;
        std::cout << "{ \"trace\": [], \"summary\": {}, \"error\": \""
                  << e.what() << "\" }" << std::endl;
    } catch (...) {
        std::cerr << "Unknown Critical Failure" << std::endl;
        std::cout << "{ \"trace\": [], \"summary\": {}, \"error\": \"Unknown\" }"
                  << std::endl;
    }
    return 0;
}
#endif


// ═══════════════════════════════════════════════════════════════════════════
//  Emscripten WebAssembly Bindings
// ═══════════════════════════════════════════════════════════════════════════
#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>

std::string runWasmSimulation(std::string inputStr, bool compareAll) {
    std::istringstream iss(inputStr);
    std::ostringstream oss;
    
    try {
        SimulationConfig cfg{};
        if (!readConfig(cfg, iss)) {
            oss << "{ \"trace\": [], \"summary\": {} }" << std::endl;
        } else {
            auto trace = readTrace(iss);
            if (trace.empty()) {
                oss << "{ \"trace\": [], \"summary\": {} }" << std::endl;
            } else {
                if (compareAll) {
                    runCompareAll(cfg, trace, oss);
                } else {
                    runSingleSimulation(cfg, trace, oss);
                }
            }
        }
    } catch (const std::exception& e) {
        oss << "{ \"trace\": [], \"summary\": {}, \"error\": \"" << e.what() << "\" }" << std::endl;
    } catch (...) {
        oss << "{ \"trace\": [], \"summary\": {}, \"error\": \"Unknown Error\" }" << std::endl;
    }
    
    return oss.str();
}

EMSCRIPTEN_BINDINGS(cache_sim) {
    emscripten::function("runWasmSimulation", &runWasmSimulation);
}
#endif
