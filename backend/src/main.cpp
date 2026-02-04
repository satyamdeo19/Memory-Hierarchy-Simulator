#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <map>
#include "MemoryHierarchy.h"

// Enum to String helper
std::string missTypeToString(MissType t) {
    switch (t) {
        case MissType::Cold: return "COLD";
        case MissType::Conflict: return "CONFLICT";
        case MissType::Capacity: return "CAPACITY";
        default: return "NONE";
    }
}

// Stats Structure
struct LevelStats {
    uint32_t hits = 0;
    uint32_t misses = 0;
    std::map<std::string, uint32_t> missTypes;
    void addMiss(MissType type) {
        misses++;
        missTypes[missTypeToString(type)]++;
    }
    void addHit() { hits++; }
};

struct SimulationStats {
    LevelStats l1, l2, l3, ram, disk;
    uint64_t totalLatency = 0;
    std::map<std::string, uint32_t> latencyHistogram;

    void record(const AccessResult& res) {
        // L1
        if (res.l1Hit) l1.addHit();
        else {
            l1.addMiss(res.events.l1MissType);
            // L2
            if (res.l2Hit) l2.addHit();
            else {
                l2.addMiss(res.events.l2MissType);
                // L3
                if (res.l3Hit) l3.addHit();
                else {
                    l3.addMiss(res.events.l3MissType);
                    // RAM
                    if (res.ramHit) ram.addHit();
                    else {
                        ram.addMiss(res.events.ramMissType);
                        // Disk (Page Fault implies Disk Hit effectively, or rather service)
                        if (res.pageFault) disk.addHit(); 
                    }
                }
            }
        }

        totalLatency += res.totalLatency;
        
        // Histogram
        std::string bucket;
        if (res.totalLatency <= 1) bucket = "1";
        else if (res.totalLatency <= 10) bucket = "2-10";
        else if (res.totalLatency <= 50) bucket = "11-50";
        else if (res.totalLatency <= 200) bucket = "51-200";
        else if (res.totalLatency <= 1000) bucket = "201-1000";
        else bucket = ">1000";
        latencyHistogram[bucket]++;
    }
};

std::string formatAccessJSON(uint32_t addrVal, 
                             AccessType type,
                             const MemoryAddress& l1Addr, 
                             const AccessResult& result) {
    std::stringstream ss;
    ss << "    {\n";
    ss << "      \"address\": \"0x" << std::hex << addrVal << "\",\n"; 
    ss << "      \"virtual_address\": \"0x" << std::hex << result.virtualAddress << "\",\n";
    ss << "      \"type\": \"" << (type == AccessType::READ ? "READ" : "WRITE") << "\",\n";
    
    ss << "      \"page_fault\": " << (result.pageFault ? "true" : "false") << ",\n";
    ss << "      \"tlb_hit\": " << (result.tlbHit ? "true" : "false") << ",\n";
    ss << "      \"tlb_latency\": " << std::dec << result.tlbLatency << ",\n";
    ss << "      \"physical_address\": \"0x" << std::hex << result.physicalAddress << "\",\n";
    ss << "      \"vpn\": \"0x" << std::hex << result.vpn << "\",\n";
    ss << "      \"ppn\": \"0x" << std::hex << result.ppn << "\",\n";
    
    ss << "      \"decomposition\": {\n";
    ss << "        \"tag\": \"0x" << std::hex << l1Addr.getTag() << "\",\n";
    ss << "        \"index\": \"0x" << std::hex << l1Addr.getIndex() << "\",\n";
    ss << "        \"offset\": \"0x" << std::hex << l1Addr.getOffset() << "\"\n";
    ss << "      },\n";
    ss << "      \"results\": [\n";
    
    // L1
    ss << "        {\n";
    ss << "          \"level\": \"L1\",\n";
    ss << "          \"hit\": " << (result.l1Hit ? "true" : "false") << ",\n";
    ss << "          \"latency\": " << (result.l1Hit ? 1 : 0) << ",\n";
    ss << "          \"miss_type\": \"" << missTypeToString(result.events.l1MissType) << "\"\n";
    ss << "        },\n";

    // L2
    ss << "        {\n";
    ss << "          \"level\": \"L2\",\n";
    ss << "          \"hit\": " << (result.l2Hit ? "true" : "false") << ",\n";
    ss << "          \"latency\": " << 0 << ",\n"; 
    ss << "          \"miss_type\": \"" << missTypeToString(result.events.l2MissType) << "\"\n";
    ss << "        },\n";

    // L3
    ss << "        {\n";
    ss << "          \"level\": \"L3\",\n";
    ss << "          \"hit\": " << (result.l3Hit ? "true" : "false") << ",\n";
    ss << "          \"latency\": " << 0 << ",\n"; 
    ss << "          \"miss_type\": \"" << missTypeToString(result.events.l3MissType) << "\"\n";
    ss << "        },\n";

    // RAM
    ss << "        {\n";
    ss << "          \"level\": \"RAM\",\n";
    ss << "          \"hit\": " << (result.ramHit ? "true" : "false") << ",\n";
    ss << "          \"latency\": " << 0 << ",\n";
    ss << "          \"miss_type\": \"" << missTypeToString(result.events.ramMissType) << "\"\n";
    ss << "        },\n";

    // DISK
    ss << "        {\n";
    ss << "          \"level\": \"DISK\",\n";
    ss << "          \"hit\": " << (result.pageFault ? "true" : "false") << ",\n";
    ss << "          \"latency\": " << 0 << "\n";
    ss << "        }\n";
    ss << "      ],\n"; 

    // Evictions
    ss << "      \"evictions\": [\n";
    for (size_t i = 0; i < result.events.evictions.size(); ++i) {
        auto& ev = result.events.evictions[i];
        ss << "        {\n";
        ss << "          \"level\": \"" << ev.first << "\",\n";
        ss << "          \"set\": \"0x" << std::hex << ev.second.set << "\",\n";
        ss << "          \"tag\": \"0x" << std::hex << ev.second.tag << "\",\n";
        ss << "          \"policy\": \"" << ev.second.policy << "\",\n";
        ss << "          \"dirty\": " << (ev.second.isDirty ? "true" : "false") << ",\n";
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

struct AccessRecord {
    std::string typeStr;
    uint32_t val;
    AccessType type;
    uint64_t nextUse;
};

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    try {
        uint32_t l1Size, l1Assoc, l2Size, l2Assoc, l3Size, l3Assoc, l3Latency, ramSize, ramAssoc, diskLatency, tlbSize, tlbLatency, blockSize, pageSize, policyID;
        size_t numAddresses;

        // Check for empty input or read failure
        if (std::cin.peek() == EOF || !(std::cin >> l1Size >> l1Assoc >> l2Size >> l2Assoc >> l3Size >> l3Assoc >> l3Latency >> ramSize >> ramAssoc >> diskLatency >> tlbSize >> tlbLatency >> blockSize >> pageSize >> policyID)) {
            std::cout << "{ \"trace\": [], \"summary\": {} }" << std::endl;
            return 0;
        }

        MemoryHierarchy memory;
        memory.initialize(l1Size, l1Assoc, l2Size, l2Assoc, l3Size, l3Assoc, l3Latency, ramSize, ramAssoc, diskLatency, tlbSize, tlbLatency, blockSize, pageSize, policyID);

        if (!(std::cin >> numAddresses)) {
             std::cout << "{ \"trace\": [], \"summary\": {} }" << std::endl;
             return 0;
        }

        uint32_t l1NumSets = l1Size / (blockSize * l1Assoc);
        
        // 1. Read entire trace
        std::vector<AccessRecord> trace;
        trace.reserve(numAddresses);
        
        for (size_t i = 0; i < numAddresses; ++i) {
            std::string lineType;
            uint32_t addrVal;
            if (!(std::cin >> lineType >> addrVal)) break;
            AccessType type = (lineType == "W" || lineType == "w") ? AccessType::WRITE : AccessType::READ;
            trace.push_back({lineType, addrVal, type, 0xFFFFFFFFFFFFFFFF});
        }
        
        // 2. Preprocess Next Use (Backwards traversal)
        std::unordered_map<uint32_t, uint64_t> lastSeen;
        for (int64_t i = trace.size() - 1; i >= 0; --i) {
            uint32_t addr = trace[i].val;
            if (lastSeen.count(addr)) {
                trace[i].nextUse = lastSeen[addr];
            }
            lastSeen[addr] = (uint64_t)i;
        }

        std::vector<std::string> jsonObjects;
        jsonObjects.reserve(trace.size());
        SimulationStats stats;

        for (size_t i = 0; i < trace.size(); ++i) {
            AccessResult res = memory.access(trace[i].val, trace[i].type, (uint64_t)i, trace[i].nextUse);
            stats.record(res);
            
            uint32_t decompAddr = res.pageFault ? 0 : res.physicalAddress;
            MemoryAddress l1Addr(decompAddr, blockSize, l1NumSets);

            jsonObjects.push_back(formatAccessJSON(trace[i].val, trace[i].type, l1Addr, res));
        }

        // OUTPUT JSON
        std::cout << "{\n";
        std::cout << "  \"trace\": [\n";
        for (size_t i = 0; i < jsonObjects.size(); ++i) {
            std::cout << jsonObjects[i];
            if (i < jsonObjects.size() - 1) {
                std::cout << ",";
            }
            std::cout << "\n";
        }
        std::cout << "  ],\n"; // End trace array

        // SUMMARY OBJECT
        std::cout << "  \"summary\": {\n";
        auto printLevel = [](const std::string& name, const LevelStats& s) {
            std::cout << "    \"" << name << "\": {\n";
            std::cout << "      \"hits\": " << s.hits << ",\n";
            std::cout << "      \"misses\": " << s.misses << ",\n";
            std::cout << "      \"miss_types\": {\n";
            int count = 0;
            for (auto const& pair : s.missTypes) {
                std::cout << "        \"" << pair.first << "\": " << pair.second;
                if (++count < s.missTypes.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "      }\n";
            std::cout << "    }";
        };

        printLevel("L1", stats.l1); std::cout << ",\n";
        printLevel("L2", stats.l2); std::cout << ",\n";
        printLevel("L3", stats.l3); std::cout << ",\n";
        printLevel("RAM", stats.ram); std::cout << ",\n";
        printLevel("DISK", stats.disk); std::cout << ",\n";

        std::cout << "    \"total_latency\": " << stats.totalLatency << ",\n";
        
        std::cout << "    \"latency_histogram\": {\n";
        int count = 0;
        for (auto const& pair : stats.latencyHistogram) {
            std::cout << "      \"" << pair.first << "\": " << pair.second;
            if (++count < stats.latencyHistogram.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "    }\n";

        std::cout << "  }\n"; // End summary
        std::cout << "}" << std::endl; // End root object

    } catch (const std::exception& e) {
        std::cerr << "Simulation Error: " << e.what() << std::endl;
        std::cout << "{ \"trace\": [], \"summary\": {}, \"error\": \"" << e.what() << "\" }" << std::endl;
        return 0; 
    } catch (...) {
        std::cerr << "Unknown Critical Failure" << std::endl;
        std::cout << "{ \"trace\": [], \"summary\": {}, \"error\": \"Unknown\" }" << std::endl;
        return 0;
    }

    return 0;
}
