import React from 'react';

function AnalyticsDashboard({ summary }) {
    if (!summary) return <div>No Analytics Data</div>;

    // Helper for Hit Rate
    const getHitRate = (levelName) => {
        const stats = summary[levelName];
        if (!stats) return "N/A";
        const total = stats.hits + stats.misses;
        if (total === 0) return "0%";
        return ((stats.hits / total) * 100).toFixed(1) + "%";
    };

    // Helper for Histogram Max Value for scaling
    const histogram = summary.latency_histogram || {};
    const maxCount = Math.max(...Object.values(histogram), 0);

    const levels = ['L1', 'L2', 'L3', 'RAM', 'DISK'];

    return (
        <div className="analytics-container">
            <div className="card">
                <h2>Simulation Analytics</h2>
                <div style={{display: 'flex', gap: '2rem', flexWrap: 'wrap'}}>
                    <div style={{flex: 1, minWidth: '300px'}}>
                        <h3>Hit Rates</h3>
                        <table style={{width: '100%', borderCollapse: 'collapse', fontFamily: 'var(--font-mono)'}}>
                            <thead>
                                <tr style={{borderBottom: '1px solid var(--surface-border)', textAlign: 'left', color: 'var(--secondary-text)'}}>
                                    <th style={{padding: '0.8rem'}}>Level</th>
                                    <th style={{padding: '0.8rem'}}>Hits</th>
                                    <th style={{padding: '0.8rem'}}>Misses</th>
                                    <th style={{padding: '0.8rem'}}>Rate</th>
                                </tr>
                            </thead>
                            <tbody>
                                {levels.map(lvl => (
                                    <tr key={lvl} style={{borderBottom: '1px solid rgba(255,255,255,0.05)', transition: 'background 0.2s'}}>
                                        <td style={{padding: '0.8rem', fontWeight: 'bold', color: 'var(--primary-color)'}}>{lvl}</td>
                                        <td style={{padding: '0.8rem', color: 'var(--hit-color)'}}>{summary[lvl]?.hits || 0}</td>
                                        <td style={{padding: '0.8rem', color: 'var(--miss-color)'}}>{summary[lvl]?.misses || 0}</td>
                                        <td style={{padding: '0.8rem', fontWeight: 'bold'}}>{getHitRate(lvl)}</td>
                                    </tr>
                                ))}
                            </tbody>
                        </table>
                    </div>

                    <div style={{flex: 1, minWidth: '300px'}}>
                        <h3>Miss Breakdown</h3>
                        {levels.slice(0, 4).map(lvl => {
                           const types = summary[lvl]?.miss_types || {};
                           if (Object.keys(types).length === 0) return null;
                           return (
                               <div key={lvl} style={{marginBottom: '1rem', background: 'rgba(0,0,0,0.2)', padding: '1rem', borderRadius: '8px'}}>
                                   <strong style={{color: 'var(--primary-color)'}}>{lvl} Misses</strong>
                                   <div style={{display: 'flex', gap: '1rem', marginTop: '0.5rem', fontFamily: 'var(--font-mono)'}}>
                                       <span style={{color: '#4fc3f7'}}>Cold: {types.COLD || 0}</span>
                                       <span style={{color: '#ffa726'}}>Conflict: {types.CONFLICT || 0}</span>
                                       <span style={{color: '#ef5350'}}>Capacity: {types.CAPACITY || 0}</span>
                                   </div>
                               </div>
                           );
                        })}
                    </div>
                </div>
            </div>

            <div className="card">
                <h3>Latency Distribution</h3>
                <div style={{marginTop: '1.5rem', fontFamily: 'var(--font-mono)'}}>
                    {Object.entries(histogram).map(([bucket, count]) => (
                        <div key={bucket} style={{marginBottom: '1rem'}}>
                            <div style={{display: 'flex', justifyContent: 'space-between', fontSize: '0.9rem', marginBottom: '0.4rem', color: 'var(--secondary-text)'}}>
                                <span>{bucket} cycles</span>
                                <span style={{color: 'var(--text-color)', fontWeight: 'bold'}}>{count}</span>
                            </div>
                            <div style={{
                                width: '100%',
                                height: '12px',
                                backgroundColor: 'rgba(255,255,255,0.05)',
                                borderRadius: '10px',
                                overflow: 'hidden',
                                border: '1px solid rgba(255,255,255,0.1)'
                            }}>
                                <div style={{
                                    height: '100%',
                                    width: `${maxCount > 0 ? (count / maxCount) * 100 : 0}%`,
                                    background: 'linear-gradient(90deg, var(--secondary-color) 0%, var(--primary-color) 100%)',
                                    transition: 'width 0.8s cubic-bezier(0.16, 1, 0.3, 1)',
                                    boxShadow: '0 0 10px rgba(100, 255, 218, 0.5)'
                                }}/>
                            </div>
                        </div>
                    ))}
                </div>
                <div style={{marginTop: '1rem', textAlign: 'right', fontSize: '1.2rem', fontWeight: 'bold'}}>
                    Total Latency: {summary.total_latency} cycles
                </div>
            </div>
        </div>
    );
}

export default AnalyticsDashboard;
