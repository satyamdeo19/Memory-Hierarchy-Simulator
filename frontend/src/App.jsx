import React, { useState, useEffect, useRef, useMemo, Fragment } from 'react'
import ComparisonPage from './ComparisonPage';
import AnalyticsDashboard from './AnalyticsDashboard';
import CacheGrid from './CacheGrid';

// ─────────────────────────────────────────────────────────────────────────────
// Valgrind .din trace parser
// Supports Valgrind lackey format:  [I|S|L|M]  hex_address,size
//   I = instruction fetch  → R
//   L = data load          → R
//   S = data store         → W
//   M = data modify (R+W)  → W
// Also accepts plain "R/W hex" lines (existing format).
// ─────────────────────────────────────────────────────────────────────────────
function parseValgrindTrace(text) {
    const MAX_LINES = 500;
    const lines = text.split('\n');
    const results = [];

    for (const raw of lines) {
        const line = raw.trim();
        if (!line || line.startsWith('#') || line.startsWith('=')) continue;

        // Valgrind lackey format: "I  04001000,1"  or " S 04f6232c,8"
        const valgrindMatch = line.match(/^([ILSMilsm])\s+([0-9a-fA-F]+)(?:,\d+)?$/);
        if (valgrindMatch) {
            const typeChar = valgrindMatch[1].toUpperCase();
            const addr     = valgrindMatch[2];
            const type     = (typeChar === 'S' || typeChar === 'M') ? 'W' : 'R';
            results.push(`${type} ${addr}`);
            if (results.length >= MAX_LINES) break;
            continue;
        }

        // Existing "R/W hex" format already accepted by the backend
        const simpleMatch = line.match(/^([RWrw])\s+([0-9a-fA-F]+)$/);
        if (simpleMatch) {
            results.push(`${simpleMatch[1].toUpperCase()} ${simpleMatch[2]}`);
            if (results.length >= MAX_LINES) break;
        }
    }
    return results.join('\n');
}

// ─────────────────────────────────────────────────────────────────────────────
// useCacheState — reconstructs the L1 cache state per step by replaying events
// Returns: Array<cacheState> where cacheState = Array[numSets] of Array[assoc]
//          Each cell: { valid: bool, tag: string, dirty: bool }
// ─────────────────────────────────────────────────────────────────────────────
function useCacheState(simulationResult, config) {
    return useMemo(() => {
        if (!simulationResult || simulationResult.length === 0) return [];

        const { l1Size, l1Assoc, blockSize } = config;
        const numSets = Math.max(1, Math.floor(l1Size / (blockSize * l1Assoc)));

        // Initialize empty cache
        const emptyState = () =>
            Array.from({ length: numSets }, () =>
                Array.from({ length: l1Assoc }, () => ({ valid: false, tag: null, dirty: false }))
            );

        // Deep-clone a state
        const cloneState = (s) => s.map(set => set.map(line => ({ ...line })));

        let state = emptyState();
        const allStates = [];

        for (const step of simulationResult) {
            state = cloneState(state);

            const setIdx  = parseInt(step.decomposition?.index || '0', 16);
            const tag     = step.decomposition?.tag || '0x0';
            const isWrite = step.type === 'WRITE';
            const l1Hit   = step.results?.find(r => r.level === 'L1')?.hit ?? false;

            if (setIdx < numSets) {
                // Apply L1 evictions from this step
                const l1Evicts = (step.evictions || []).filter(e => e.level === 'L1');
                for (const ev of l1Evicts) {
                    const eSet = parseInt(ev.set, 16);
                    const eTag = ev.tag;
                    if (eSet < numSets) {
                        const idx = state[eSet].findIndex(
                            l => l.valid && parseInt(l.tag, 16) === parseInt(eTag, 16)
                        );
                        if (idx !== -1) state[eSet][idx] = { valid: false, tag: null, dirty: false };
                    }
                }

                if (l1Hit) {
                    // Update dirty bit on write hit
                    const wayIdx = state[setIdx].findIndex(
                        l => l.valid && parseInt(l.tag, 16) === parseInt(tag, 16)
                    );
                    if (wayIdx !== -1 && isWrite) {
                        state[setIdx][wayIdx].dirty = true;
                    }
                } else {
                    // L1 miss — install the new block
                    let slot = state[setIdx].findIndex(l => !l.valid);
                    if (slot === -1) slot = 0; // all ways occupied, overwrite first (eviction handled above)
                    state[setIdx][slot] = { valid: true, tag, dirty: isWrite };
                }
            }

            allStates.push(state);
        }
        return allStates;
    }, [simulationResult, config]);
}

// ─────────────────────────────────────────────────────────────────────────────
// App Component
// ─────────────────────────────────────────────────────────────────────────────
function App() {
    const [config, setConfig] = useState({
        l1Size:      1024,
        l1Assoc:     2,
        l2Size:      4096,
        l2Assoc:     4,
        l3Size:      16384,
        l3Assoc:     8,
        l3Latency:   20,
        ramSize:     1048576,
        ramAssoc:    16,
        diskLatency: 10000,
        blockSize:   64,
        addresses:   "# --- CACHE SIMULATOR TUTORIAL ---\n# \n# 1. COLD MISSES & CACHE HITS\nR 0000\nW 0040\nR 0000\n\n# 2. ADVANCED PATTERN\n# This specific access pattern causes LRU, FIFO,\n# LFU, and Optimal policies to perform differently!\n# Run the 'Compare Policies' tab to see it!\nR 0100\nR 0140\nR 0180\nR 01C0\nR 0100\nR 0140\nR 0200\nR 0100\nR 0140\nR 0180\nR 01C0\nR 0200"
    });

    const [activeTab,        setActiveTab]        = useState('simulate');
    const [simulationResult, setSimulationResult] = useState(null);
    const [summary,          setSummary]          = useState(null);
    const [step,             setStep]             = useState(0);
    const [loading,          setLoading]          = useState(false);
    const [error,            setError]            = useState(null);
    const [isPlaying,        setIsPlaying]        = useState(false);
    const [traceWarning,     setTraceWarning]     = useState(null);
    const playInterval = useRef(null);
    const fileInputRef  = useRef(null);

    // ── Simulation runner ─────────────────────────────────────────────────────
    const handleRun = async () => {
        setLoading(true);
        setError(null);
        setIsPlaying(false);
        try {
            const addrList = config.addresses.split('\n').filter(s => s.trim() !== '' && !s.trim().startsWith('#'));
            
            // Build the string that C++ std::cin expects
            const tlbSize = 64;
            const tlbLatency = 10;
            const pageSize = 4096;
            const policyID = 0; // default LRU
            
            let inputStr = `${config.l1Size} ${config.l1Assoc} ${config.l2Size} ${config.l2Assoc} ${config.l3Size} ${config.l3Assoc} ${config.l3Latency} ${config.ramSize} ${config.ramAssoc} ${config.diskLatency} ${tlbSize} ${tlbLatency} ${config.blockSize} ${pageSize} ${policyID}\n`;
            inputStr += `${addrList.length}\n`;
            inputStr += addrList.join('\n');

            if (!window.Module || !window.Module.runWasmSimulation) {
                throw new Error("Simulation engine is still loading. Please try again in a few seconds.");
            }

            const rawJson = window.Module.runWasmSimulation(inputStr, false);
            const data = JSON.parse(rawJson);
            
            if (data.error) throw new Error(data.error);

            if (data.trace) {
                setSimulationResult(data.trace);
                setSummary(data.summary);
            } else {
                setSimulationResult(data);
                setSummary(null);
            }
            setStep(0);
        } catch (err) {
            setError(err.message);
        } finally {
            setLoading(false);
        }
    };

    // ── Step controls ─────────────────────────────────────────────────────────
    const nextStep = () => {
        if (simulationResult && step < simulationResult.length - 1) {
            setStep(s => s + 1);
        } else {
            setIsPlaying(false);
        }
    };
    const prevStep   = () => { if (step > 0) setStep(s => s - 1); };
    const togglePlay = () => setIsPlaying(p => !p);

    useEffect(() => {
        if (isPlaying) {
            playInterval.current = setInterval(nextStep, 1000);
        } else {
            clearInterval(playInterval.current);
        }
        return () => clearInterval(playInterval.current);
    }, [isPlaying, step, simulationResult]);

    useEffect(() => {
        const handleKeyDown = (e) => {
            if (activeTab === 'simulate') {
                if (e.key === 'ArrowRight') nextStep();
                if (e.key === 'ArrowLeft')  prevStep();
                if (e.key === ' ')          togglePlay();
            }
        };
        window.addEventListener('keydown', handleKeyDown);
        return () => window.removeEventListener('keydown', handleKeyDown);
    }, [step, simulationResult, isPlaying, activeTab]);

    // ── Trace file import ─────────────────────────────────────────────────────
    const handleFileImport = (e) => {
        const file = e.target.files[0];
        if (!file) return;
        setTraceWarning(null);

        const reader = new FileReader();
        reader.onload = (evt) => {
            const text   = evt.target.result;
            const parsed = parseValgrindTrace(text);
            const count  = parsed.split('\n').filter(l => l.trim()).length;

            if (count === 0) {
                setTraceWarning('No valid trace lines found in file.');
                return;
            }
            if (count >= 500) {
                setTraceWarning(`Trace truncated to 500 accesses (file had more).`);
            }
            setConfig(c => ({ ...c, addresses: parsed }));
        };
        reader.readAsText(file);
        // Reset input so the same file can be re-imported
        e.target.value = '';
    };

    // ── Cache state reconstruction ────────────────────────────────────────────
    const numSets     = Math.max(1, Math.floor(config.l1Size / (config.blockSize * config.l1Assoc)));
    const cacheStates = useCacheState(simulationResult, config);

    // ── Current step helpers ──────────────────────────────────────────────────
    const currentStepData = simulationResult ? simulationResult[step] : null;
    const currentCache    = cacheStates[step] || null;

    const getLevelResult = (levelName) => {
        if (!currentStepData) return null;
        return currentStepData.results.find(r => r.level === levelName);
    };

    const getLevelStatus = (levelName) => {
        const res = getLevelResult(levelName);
        if (levelName === 'L1') return res && res.hit ? 'hit' : 'miss';
        if (levelName === 'L2') {
            const l1 = getLevelResult('L1');
            if (l1 && !l1.hit) return res && res.hit ? 'hit' : 'miss';
        }
        if (levelName === 'L3') {
            const l1 = getLevelResult('L1');
            const l2 = getLevelResult('L2');
            if (l1 && !l1.hit && l2 && !l2.hit) return res && res.hit ? 'hit' : 'miss';
        }
        if (levelName === 'RAM') {
            const l1 = getLevelResult('L1');
            const l2 = getLevelResult('L2');
            const l3 = getLevelResult('L3');
            if (l1 && !l1.hit && l2 && !l2.hit && l3 && !l3.hit) {
                const ram = getLevelResult('RAM');
                if (ram) return ram.hit ? 'hit' : 'miss';
            }
        }
        if (levelName === 'DISK') {
            const ram = getLevelResult('RAM');
            if (ram && !ram.hit) return 'hit';
        }
        return 'idle';
    };

    const getMissExplanation = (level) => {
        const res = getLevelResult(level);
        if (!res || res.hit) return null;
        switch (res.miss_type) {
            case 'COLD':     return 'Cold Miss';
            case 'CONFLICT': return 'Conflict Miss';
            case 'CAPACITY': return 'Capacity Miss';
            default:         return 'Miss';
        }
    };

    const getEvictionInfo = (level) => {
        if (!currentStepData || !currentStepData.evictions) return null;
        const evicts = currentStepData.evictions.filter(e => e.level === level);
        if (evicts.length === 0) return null;
        return evicts.map((e, idx) => (
            <div key={idx} className="eviction-alert">
                Evicted Line<br />
                Tag: {e.tag} (Set {e.set})<br />
                Dirty: {e.dirty ? 'YES' : 'No'}<br />
                {e.next_use_distance && (
                    <span style={{ fontSize: '0.8em', color: '#ffd700' }}>
                        Next Use: {e.next_use_distance}
                    </span>
                )}
            </div>
        ));
    };

    const getExplanationPanel = (level) => {
        const status = getLevelStatus(level);
        if (status === 'idle') return null;
        const res       = getLevelResult(level);
        const evictions = getEvictionInfo(level);
        const missExpl  = getMissExplanation(level);
        return (
            <div className="explanation-tooltip">
                <strong>{status.toUpperCase()}</strong>
                {res && res.latency > 0 && <div>+{res.latency} cycles</div>}
                {missExpl  && <p>{missExpl}</p>}
                {evictions}
            </div>
        );
    };

    const exportAnalytics = () => {
        if (!summary) return;
        const dataStr = 'data:text/json;charset=utf-8,' +
            encodeURIComponent(JSON.stringify(summary, null, 2));
        const a = document.createElement('a');
        a.setAttribute('href', dataStr);
        a.setAttribute('download', 'simulation_analytics.json');
        document.body.appendChild(a);
        a.click();
        a.remove();
    };

    // Active set/tag for the CacheGrid highlight
    const activeSet = currentStepData
        ? parseInt(currentStepData.decomposition?.index || '0', 16)
        : null;
    const activeTag = currentStepData?.decomposition?.tag || null;
    const evictedTag = currentStepData?.evictions?.find(e => e.level === 'L1')?.tag || null;

    // ── Render ────────────────────────────────────────────────────────────────
    return (
        <div className="container">
            <div className="header" style={{ marginBottom: '2rem' }}>
                <h1 style={{ color: 'var(--primary-color)' }}>CPU Cache & Memory Hierarchy Simulator</h1>
                <p style={{ maxWidth: '800px', margin: '0 auto', color: 'var(--secondary-text)', lineHeight: '1.6', fontSize: '1.1rem' }}>
                    An interactive educational tool that visualizes how modern CPUs manage data. 
                    Configure your cache parameters, input a memory trace, and watch how data flows between <strong>L1, L2, L3 caches, RAM, and Disk</strong>. 
                    Explore virtual memory mapping, TLB hits, and compare how different replacement policies (LRU, FIFO, etc.) affect your system's performance.
                </p>
            </div>

            <div className="mode-tabs">
                {['simulate', 'analytics', 'compare'].map(tab => (
                    <button
                        key={tab}
                        className={`mode-tab ${activeTab === tab ? 'active' : ''}`}
                        onClick={() => setActiveTab(tab)}
                    >
                        {tab === 'simulate' ? 'Visualizer' : tab === 'analytics' ? 'Analytics' : 'Compare Policies'}
                    </button>
                ))}
            </div>

            <div className={activeTab === 'simulate' ? "app-layout" : ""}>
                {/* ── Left Sidebar: Config & Trace (Only in Simulator Mode) ── */}
                {activeTab === 'simulate' && (
                    <div className="sidebar">
                        <div className="card glass-panel">
                            <h2>⚙️ Configuration</h2>
                            <div className="config-grid">
                                {[
                                    ['L1 Size',     'l1Size'],
                                    ['L1 Assoc',    'l1Assoc'],
                                    ['L2 Size',     'l2Size'],
                                    ['L2 Assoc',    'l2Assoc'],
                                    ['L3 Size',     'l3Size'],
                                    ['RAM Size',    'ramSize'],
                                    ['Disk Latency','diskLatency'],
                                    ['Block Size',  'blockSize'],
                                ].map(([label, key]) => (
                                    <div key={key} className="input-group">
                                        <label>{label}</label>
                                        <input
                                            type="number"
                                            value={config[key]}
                                            onChange={e => setConfig({ ...config, [key]: parseInt(e.target.value) })}
                                        />
                                    </div>
                                ))}
                            </div>
                            <button className="primary-btn" onClick={handleRun} disabled={loading}>
                                {loading ? 'SIMULATING...' : 'RUN SIMULATION'}
                            </button>
                            {error && <div style={{ color: 'var(--miss-color)', marginTop: '1rem' }}>{error}</div>}
                        </div>

                        <div className="card glass-panel">
                            <h2>📜 Access Trace</h2>
                            <div className="trace-import-row">
                                <button
                                    className="file-upload-btn"
                                    onClick={() => fileInputRef.current?.click()}
                                    title="Import Valgrind .din or plain R/W trace file"
                                >
                                    📂 Import File
                                </button>
                                <input
                                    ref={fileInputRef}
                                    type="file"
                                    accept=".din,.txt,.trace"
                                    style={{ display: 'none' }}
                                    onChange={handleFileImport}
                                />
                            </div>
                            <span className="trace-import-hint">Accepts .din, Valgrind lackey, or R/W text</span>
                            
                            {traceWarning && (
                                <div className="trace-warning">{traceWarning}</div>
                            )}

                            <div className="input-group" style={{ marginTop: '1rem' }}>
                                <textarea
                                    value={config.addresses}
                                    onChange={e => setConfig({ ...config, addresses: e.target.value })}
                                    placeholder="e.g.  R 8000&#10;W 1A00"
                                />
                            </div>
                            <div style={{ fontSize: '0.8rem', color: 'var(--secondary-text)', marginTop: '0.4rem', textAlign: 'right' }}>
                                {config.addresses.split('\n').filter(l => l.trim()).length} accesses (max 500 for UI)
                            </div>
                        </div>
                    </div>
                )}

                {/* ── Main Content Area ── */}
                <div className="main-content">
                    {activeTab === 'compare' ? (
                        <div className="glass-panel"><ComparisonPage config={config} /></div>
                    ) : activeTab === 'analytics' ? (
                        simulationResult && summary ? (
                            <>
                                <div style={{ textAlign: 'right', marginBottom: '1rem' }}>
                                    <button className="nav-btn" onClick={exportAnalytics}>Export JSON</button>
                                </div>
                                <div className="glass-panel card"><AnalyticsDashboard summary={summary} /></div>
                            </>
                        ) : (
                            <div className="card glass-panel" style={{textAlign: 'center', padding: '4rem'}}>
                                <h2>No Data</h2>
                                <p style={{color: 'var(--secondary-text)'}}>Run a simulation first to view analytics.</p>
                            </div>
                        )
                    ) : (
                        simulationResult && currentStepData && (
                            <div className="card glass-panel">
                                {/* Header row */}
                                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '2rem' }}>
                                    <div>
                                        <span className="latency-label">Operation</span>
                                        <div style={{ fontSize: '2rem', color: currentStepData.type === 'WRITE' ? 'var(--accent-color)' : 'var(--primary-color)', fontFamily: 'var(--font-heading)', fontWeight: 'bold' }}>
                                            {currentStepData.type}{' '}
                                            <span style={{ color: 'var(--text-color)' }}>
                                                0x{parseInt(currentStepData.address).toString(16).toUpperCase()}
                                            </span>
                                        </div>
                                    </div>
                                    <div>
                                        <span className="latency-label">Address Decomposition</span>
                                        <div style={{ fontFamily: 'var(--font-mono)', fontSize: '1.1rem', color: 'var(--secondary-text)' }}>
                                            Tag: <span className="text-cyan">{currentStepData.decomposition.tag}</span>
                                            &nbsp;|&nbsp;Idx: <span className="text-orange">{currentStepData.decomposition.index}</span>
                                            &nbsp;|&nbsp;Off: {currentStepData.decomposition.offset}
                                        </div>
                                    </div>
                                    <div>
                                        <span className="latency-label">Virtual Memory</span>
                                        <div style={{ fontFamily: 'var(--font-mono)', fontSize: '1rem', color: 'var(--secondary-text)' }}>
                                            VPN: {currentStepData.vpn}
                                            &nbsp;→&nbsp;PPN: {currentStepData.ppn}
                                            {currentStepData.tlb_hit
                                                ? <span className="text-green">&nbsp;[TLB HIT]</span>
                                                : <span className="text-orange">&nbsp;[TLB MISS]</span>}
                                            {currentStepData.page_fault &&
                                                <span className="text-red">&nbsp;⚠ PAGE FAULT</span>}
                                        </div>
                                    </div>
                                </div>

                                {/* Memory hierarchy visualization */}
                                <div className="visualization">
                                    {['L1', 'L2', 'L3', 'RAM', 'DISK'].map((level, i, arr) => (
                                        <Fragment key={level}>
                                            <div className={`mem-level ${getLevelStatus(level)}`}>
                                                <h3>{level === 'DISK' ? 'Disk' : level}</h3>
                                                <div className="status-badge">
                                                    {getLevelStatus(level) === 'idle'
                                                        ? (level === 'L1' ? 'MISS' : '—')
                                                        : level === 'DISK' ? 'ACCESS' : getLevelStatus(level).toUpperCase()}
                                                </div>
                                                {getExplanationPanel(level)}
                                                {level === 'DISK' && getLevelStatus('DISK') !== 'idle' && (
                                                    <div className="explanation-tooltip">
                                                        Fetch from Disk<br />+{config.diskLatency} cycles
                                                    </div>
                                                )}
                                            </div>
                                            {i < arr.length - 1 && <div className={`arrow ${getLevelStatus(arr[i+1]) !== 'idle' ? 'active' : ''}`}>→</div>}
                                        </Fragment>
                                    ))}
                                </div>

                                <div className="latency-display">
                                    {currentStepData.total_latency}{' '}
                                    <span className="latency-label">Total Cycles</span>
                                </div>

                                {/* ── L1 Cache State Grid ── */}
                                {currentCache && (
                                    <div style={{ marginTop: '2rem', borderTop: '1px solid rgba(255,255,255,0.05)', paddingTop: '1rem' }}>
                                        <CacheGrid
                                            cacheState={currentCache}
                                            numSets={numSets}
                                            associativity={config.l1Assoc}
                                            activeSet={activeSet}
                                            activeTag={activeTag}
                                            evictedTag={evictedTag}
                                        />
                                    </div>
                                )}

                                {/* Step Controls */}
                                <div className="execution-panel">
                                    <div className="nav-controls">
                                        <button className="nav-btn" onClick={prevStep} disabled={step === 0}>
                                            ← Previous
                                        </button>
                                        <button className="nav-btn" onClick={togglePlay} style={{ minWidth: '100px', borderColor: 'var(--primary-color)', color: 'var(--primary-color)' }}>
                                            {isPlaying ? 'Pause' : 'Play ▶'}
                                        </button>
                                        <button className="nav-btn" onClick={nextStep} disabled={step === simulationResult.length - 1}>
                                            Next →
                                        </button>
                                    </div>
                                    <div className="step-info" style={{ marginTop: '0.8rem' }}>
                                        Step {step + 1} of {simulationResult.length}
                                    </div>
                                </div>
                            </div>
                        )
                    )}
                </div>
            </div>
        </div>
    );
}

export default App;
