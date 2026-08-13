import { useState } from 'react';

function ComparisonPage({ config }) {
    const [results, setResults] = useState(null);
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState(null);

    const handleCompare = async () => {
        setLoading(true);
        setError(null);
        try {
            const addrList = config.addresses.split('\n').filter(s => s.trim() !== "" && !s.trim().startsWith('#'));
            
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

            // Note: Second parameter 'true' triggers compareAll mode
            const rawJson = window.Module.runWasmSimulation(inputStr, true);
            const data = JSON.parse(rawJson);
            if (data.error) throw new Error(data.error);
            
            // Map the raw WebAssembly JSON into the format the UI expects
            const totalAccesses = addrList.length;
            const comparison = data.results.map(res => {
                const l1Hits = res.summary.L1.hits;
                const hitRate = totalAccesses > 0 ? (l1Hits / totalAccesses) : 0;
                const avgLatency = totalAccesses > 0 ? (res.summary.total_latency / totalAccesses).toFixed(2) : 0;
                
                // Count actual evictions (capacity + conflict misses usually lead to evictions)
                const evictionCount = (res.summary.L1.miss_types.CAPACITY || 0) + (res.summary.L1.miss_types.CONFLICT || 0);
                
                return {
                    policy: res.policy,
                    hitRate: hitRate,
                    avgLatency: parseFloat(avgLatency),
                    evictionCount: evictionCount
                };
            });
            
            let bestPolicy = comparison[0]?.policy || "LRU";
            let lowestLatency = comparison[0]?.avgLatency || 999999;
            for (const p of comparison) {
                if (p.avgLatency < lowestLatency) {
                    lowestLatency = p.avgLatency;
                    bestPolicy = p.policy;
                }
            }
            
            setResults({ bestPolicy, comparison });
        } catch (err) {
            setError(err.message);
        } finally {
            setLoading(false);
        }
    };

    if (loading) return <div className="card"><h3>Running Comparison... (x4 policies)</h3></div>;

    const maxLatency = results ? Math.max(...results.comparison.map(r => r.avgLatency)) : 0;

    return (
        <div className="comparison-container">
            <div className="card glass-panel" style={{ textAlign: 'center' }}>
                <h2 style={{ marginBottom: '0.5rem' }}>🎯 Policy Comparison</h2>
                <p style={{ color: 'var(--secondary-text)', maxWidth: '600px', margin: '0 auto 2rem auto', lineHeight: '1.6' }}>
                    This tool runs your configured trace against <strong>four different Cache Replacement Policies</strong> (LRU, FIFO, LFU, and Random) simultaneously. 
                    It allows you to empirically observe which policy yields the highest Hit Rate and lowest Average Latency for your specific memory access pattern.
                </p>
                <button className="primary-btn" onClick={handleCompare} disabled={loading} style={{ maxWidth: '300px' }}>
                    {loading ? 'RUNNING...' : 'RUN COMPARISON'}
                </button>
                {error && <div style={{ color: 'var(--miss-color)', marginTop: '1rem' }}>{error}</div>}
            </div>

            {results && (
                <>
                    <div className="card glass-panel">
                        <h3 style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
                            🏆 Best Policy: <span style={{color: 'var(--primary-color)', fontSize: '1.4rem'}}>{results.bestPolicy}</span>
                        </h3>
                        
                        <table style={{width: '100%', textAlign: 'left', borderCollapse: 'collapse', marginTop: '1rem', fontFamily: 'var(--font-mono)'}}>
                            <thead>
                                <tr style={{borderBottom: '1px solid var(--surface-border)', color: 'var(--secondary-text)'}}>
                                    <th style={{padding: '1rem'}}>Policy</th>
                                    <th style={{padding: '1rem'}}>Hit Rate</th>
                                    <th style={{padding: '1rem'}}>Avg Latency</th>
                                    <th style={{padding: '1rem'}}>Evictions</th>
                                </tr>
                            </thead>
                            <tbody>
                                {results.comparison.map(p => (
                                    <tr key={p.policy} style={{borderBottom: '1px solid rgba(255,255,255,0.05)', transition: 'background 0.2s'}}>
                                        <td style={{padding: '1rem', fontWeight: 'bold', color: p.policy === results.bestPolicy ? 'var(--primary-color)' : 'var(--text-color)'}}>
                                            {p.policy} {p.policy === results.bestPolicy && '⭐'}
                                        </td>
                                        <td style={{padding: '1rem', color: 'var(--hit-color)'}}>{(p.hitRate * 100).toFixed(1)}%</td>
                                        <td style={{padding: '1rem'}}>{p.avgLatency} cyc</td>
                                        <td style={{padding: '1rem', color: 'var(--miss-color)'}}>{p.evictionCount}</td>
                                    </tr>
                                ))}
                            </tbody>
                        </table>
                    </div>

                    <div className="card glass-panel">
                        <h3>Latency Comparison <span style={{fontSize: '0.9rem', color: 'var(--secondary-text)', fontWeight: 'normal'}}>(Lower is Better)</span></h3>
                        <div style={{display: 'flex', flexDirection: 'column', gap: '1.5rem', marginTop: '1.5rem', fontFamily: 'var(--font-mono)'}}>
                            {results.comparison.map(p => (
                                <div key={p.policy}>
                                    <div style={{display: 'flex', justifyContent: 'space-between', marginBottom: '0.5rem', color: 'var(--secondary-text)'}}>
                                        <span style={{color: p.policy === results.bestPolicy ? 'var(--primary-color)' : 'var(--text-color)'}}>{p.policy}</span>
                                        <span>{p.avgLatency} cycles</span>
                                    </div>
                                    <div style={{
                                        height: '12px', 
                                        width: '100%', 
                                        backgroundColor: 'rgba(255,255,255,0.05)',
                                        borderRadius: '10px',
                                        overflow: 'hidden',
                                        border: '1px solid rgba(255,255,255,0.1)'
                                    }}>
                                        <div style={{
                                            height: '100%',
                                            width: `${(p.avgLatency / maxLatency) * 100}%`,
                                            background: p.policy === results.bestPolicy ? 'linear-gradient(90deg, var(--secondary-color) 0%, var(--primary-color) 100%)' : 'rgba(255,255,255,0.2)',
                                            transition: 'width 0.8s cubic-bezier(0.16, 1, 0.3, 1)',
                                            boxShadow: p.policy === results.bestPolicy ? '0 0 10px rgba(100, 255, 218, 0.5)' : 'none'
                                        }} />
                                    </div>
                                </div>
                            ))}
                        </div>
                    </div>
                </>
            )}
        </div>
    );
}

export default ComparisonPage;
