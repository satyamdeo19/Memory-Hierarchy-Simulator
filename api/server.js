const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
const { spawn } = require('child_process');
const path = require('path');

const app = express();
app.use(cors());
app.use(bodyParser.json());

// Helper: Format Input
const formatAddresses = (addresses) => {
    return addresses.map(line => {
        let parts = line.toString().trim().split(/\s+/);
        let type = 'R';
        let addrStr = '';
        
        if (parts.length === 2) {
            type = parts[0].toUpperCase();
            addrStr = parts[1];
        } else if (parts.length === 1) {
            addrStr = parts[0];
        } else {
            return null; 
        }
        
        // Parse hex/int
        let addrVal = parseInt(addrStr, 16);
        if (isNaN(addrVal)) return null;
        
        return `${type} ${addrVal}`;
    }).filter(x => x !== null);
};

// Helper: Run Simulation
const runSimulation = (config, addresses, policyID) => {
    return new Promise((resolve, reject) => {
        const { 
            l1Size, l1Assoc, 
            l2Size, l2Assoc, 
            l3Size = 8192, l3Assoc = 8, l3Latency = 20, 
            ramSize = 1048576, ramAssoc = 16, diskLatency = 10000,
            tlbSize = 16, tlbLatency = 1,
            blockSize, pageSize = 4096 
        } = config;

        const formattedLines = formatAddresses(addresses);
        const inputData = [
            `${l1Size} ${l1Assoc} ${l2Size} ${l2Assoc} ${l3Size} ${l3Assoc} ${l3Latency} ${ramSize} ${ramAssoc} ${diskLatency} ${tlbSize} ${tlbLatency} ${blockSize} ${pageSize} ${policyID}`,
            `${formattedLines.length}`,
            ...formattedLines
        ].join('\n');

        const exePath = path.resolve(__dirname, '../backend/build/memory_sim.exe');
        const simProcess = spawn(exePath);
        
        let stdoutData = '';
        let stderrData = '';

        simProcess.stdout.on('data', (data) => stdoutData += data.toString());
        simProcess.stderr.on('data', (data) => stderrData += data.toString());

        simProcess.on('close', (code) => {
            if (code !== 0) {
                return reject({ message: "Simulator failed", details: stderrData });
            }
            try {
                // Try finding Outer JSON Object first
                const jsonStart = stdoutData.indexOf('{');
                const jsonEnd = stdoutData.lastIndexOf('}');
                
                if (jsonStart !== -1 && jsonEnd !== -1) {
                    const jsonStr = stdoutData.substring(jsonStart, jsonEnd + 1);
                    const result = JSON.parse(jsonStr);
                    resolve(result); // Returns { trace: [], summary: {} }
                } else {
                    // Fallback to Array
                    const arrStart = stdoutData.indexOf('[');
                    const arrEnd = stdoutData.lastIndexOf(']');
                     if (arrStart !== -1 && arrEnd !== -1) {
                         const jsonStr = stdoutData.substring(arrStart, arrEnd + 1);
                         const result = JSON.parse(jsonStr);
                         resolve({ trace: result, summary: {} }); // Wrap it
                     } else {
                        throw new Error("No JSON found in output");
                     }
                }
            } catch (e) {
                reject({ message: "JSON Parse Error", raw: stdoutData });
            }
        });

        simProcess.stdin.write(inputData);
        simProcess.stdin.end();
    });
};

// Main Endpoint: Returns { trace, summary }
app.post('/simulate', async (req, res) => {
    try {
        const result = await runSimulation(req.body, req.body.addresses, req.body.policyID || 0);
        res.json(result);
    } catch (e) {
        res.status(500).json({ error: e.message, details: e.details });
    }
});

// Comparison Endpoint: Uses summary data
app.post('/compare', async (req, res) => {
    const policies = [
        { id: 0, name: "LRU" },
        { id: 1, name: "FIFO" },
        { id: 2, name: "LFU" },
        { id: 3, name: "Optimal" }
    ];

    try {
        const results = await Promise.all(policies.map(async (p) => {
            const data = await runSimulation(req.body, req.body.addresses, p.id);
            const summary = data.summary || {};
            const trace = data.trace || [];
            
            // Use summary if available, else calc
            let hitRate = 0;
            let avgLatency = 0;
            let evictions = 0;
            const totalAccesses = trace.length;

            if (Object.keys(summary).length > 0) {
                // Hits: Sum L1, L2, L3 Hits? 
                // Summary separates them.
                // Hit Rate = (L1.hits + L2.hits + L3.hits) / totalAccesses?
                // Wait, if L1 hit, L2 is not accessed.
                // So Total Hits = L1 hits + L2 hits + L3 hits + RAM hits?
                // Typically Hit Rate implies Cache Hit Rate.
                // Let's define "Hit" as serviced by L1/L2/L3.
                // Or just L1?
                // Comparison page used "Any Level Hit" logic.
                // Let's stick to System Hit Rate (Any Cache Level).
                // RAM is technically memory, not cache (in CPU context), but this sim treats it as cache.
                // Let's exclude DISK (Page Fault).
                
                const hits = (summary.L1?.hits || 0) + (summary.L2?.hits || 0) + (summary.L3?.hits || 0); // + (summary.RAM?.hits || 0);
                // Actually previous logic iterated levels ['L1', 'L2', 'L3'].
                
                evictions = trace.reduce((acc, item) => acc + item.evictions.length, 0); // Summary doesn't have total evictions explicitly?
                // Wait, summary has stats but maybe not eviction count?
                // Checks summary content... main.cpp Summary DOES NOT have eviction count.
                // Only hits/misses/types.
                // So for eviction count I must traverse trace or add it to summary.
                // Traversing trace is fine for now.
                
                hitRate = totalAccesses > 0 ? (hits / totalAccesses) : 0;
                avgLatency = totalAccesses > 0 ? (summary.total_latency / totalAccesses) : 0;

            } else {
                // Legacy Fallback (shouldn't happen with new backend)
                let totalHits = 0;
                let totalLatency = 0;
                for (const item of trace) {
                     let isHit = false;
                     for (const level of ['L1', 'L2', 'L3']) {
                        const r = item.results.find(res => res.level === level);
                        if (r && r.hit) { isHit = true; break; }
                     }
                     if (isHit) totalHits++;
                     totalLatency += item.total_latency;
                     evictions += item.evictions.length;
                }
                hitRate = totalAccesses > 0 ? (totalHits / totalAccesses) : 0;
                avgLatency = totalAccesses > 0 ? (totalLatency / totalAccesses) : 0;
            }

            return {
                policy: p.name,
                hitRate: parseFloat(hitRate.toFixed(4)), 
                avgLatency: parseFloat(avgLatency.toFixed(2)),
                evictionCount: evictions,
                totalAccesses
            };
        }));
        
        let bestPolicy = results[0];
        for (const r of results) {
            if (r.avgLatency < bestPolicy.avgLatency) bestPolicy = r;
            else if (r.avgLatency === bestPolicy.avgLatency && r.hitRate > bestPolicy.hitRate) bestPolicy = r;
        }

        res.json({
            comparison: results,
            bestPolicy: bestPolicy.policy
        });

    } catch (e) {
        console.error("Comparison Error", e);
        res.status(500).json({ error: "Comparison failed", details: e });
    }
});

const PORT = 3001;
app.listen(PORT, () => {
    console.log(`API Server running on port ${PORT}`);
});
