import { useState } from 'react';

function ComparisonPage({ config }) {
    const [results, setResults] = useState(null);
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState(null);

    const handleCompare = async () => {
        setLoading(true);
        setError(null);
        try {
            const addrList = config.addresses.split('\n').filter(s => s.trim() !== "");
            const response = await fetch('http://localhost:3001/compare', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    ...config,
                    addresses: addrList
                })
            });
            const data = await response.json();
            if (data.error) throw new Error(data.error);
            setResults(data);
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
            <div className="card">
                <h2>Policy Comparison</h2>
                <button className="primary-btn" onClick={handleCompare}>
                    Run Comparison
                </button>
                {error && <div style={{ color: 'red', marginTop: '1rem' }}>{error}</div>}
            </div>

            {results && (
                <>
                    <div className="card">
                        <h3>Best Policy: <span style={{color: '#00e676'}}>{results.bestPolicy}</span></h3>
                        
                        <table style={{width: '100%', textAlign: 'left', borderCollapse: 'collapse', marginTop: '1rem'}}>
                            <thead>
                                <tr style={{borderBottom: '1px solid #444'}}>
                                    <th style={{padding: '0.5rem'}}>Policy</th>
                                    <th style={{padding: '0.5rem'}}>Hit Rate</th>
                                    <th style={{padding: '0.5rem'}}>Avg Latency</th>
                                    <th style={{padding: '0.5rem'}}>Evictions</th>
                                </tr>
                            </thead>
                            <tbody>
                                {results.comparison.map(p => (
                                    <tr key={p.policy} style={{borderBottom: '1px solid #333'}}>
                                        <td style={{padding: '0.5rem', fontWeight: 'bold'}}>{p.policy}</td>
                                        <td style={{padding: '0.5rem'}}>{(p.hitRate * 100).toFixed(1)}%</td>
                                        <td style={{padding: '0.5rem'}}>{p.avgLatency} cyc</td>
                                        <td style={{padding: '0.5rem'}}>{p.evictionCount}</td>
                                    </tr>
                                ))}
                            </tbody>
                        </table>
                    </div>

                    <div className="card">
                        <h3>Latency Comparison (Lower is Better)</h3>
                        <div style={{display: 'flex', flexDirection: 'column', gap: '1rem', marginTop: '1rem'}}>
                            {results.comparison.map(p => (
                                <div key={p.policy}>
                                    <div style={{display: 'flex', justifyContent: 'space-between', marginBottom: '0.25rem'}}>
                                        <span>{p.policy}</span>
                                        <span>{p.avgLatency}</span>
                                    </div>
                                    <div style={{
                                        height: '24px', 
                                        width: '100%', 
                                        backgroundColor: '#333',
                                        borderRadius: '4px',
                                        overflow: 'hidden'
                                    }}>
                                        <div style={{
                                            height: '100%',
                                            width: `${(p.avgLatency / maxLatency) * 100}%`,
                                            backgroundColor: p.policy === results.bestPolicy ? '#00e676' : '#29b6f6',
                                            transition: 'width 0.5s ease'
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
