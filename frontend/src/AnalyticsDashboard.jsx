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
                        <table style={{width: '100%', borderCollapse: 'collapse'}}>
                            <thead>
                                <tr style={{borderBottom: '1px solid #444', textAlign: 'left'}}>
                                    <th style={{padding: '0.5rem'}}>Level</th>
                                    <th style={{padding: '0.5rem'}}>Hits</th>
                                    <th style={{padding: '0.5rem'}}>Misses</th>
                                    <th style={{padding: '0.5rem'}}>Rate</th>
                                </tr>
                            </thead>
                            <tbody>
                                {levels.map(lvl => (
                                    <tr key={lvl} style={{borderBottom: '1px solid #333'}}>
                                        <td style={{padding: '0.5rem', fontWeight: 'bold'}}>{lvl}</td>
                                        <td style={{padding: '0.5rem'}}>{summary[lvl]?.hits || 0}</td>
                                        <td style={{padding: '0.5rem'}}>{summary[lvl]?.misses || 0}</td>
                                        <td style={{padding: '0.5rem'}}>{getHitRate(lvl)}</td>
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
                               <div key={lvl} style={{marginBottom: '1rem'}}>
                                   <strong>{lvl} Misses:</strong>
                                   <div style={{display: 'flex', gap: '1rem', marginTop: '0.25rem'}}>
                                       <span style={{color: '#4fc3f7'}}>Cold: {types.COLD || 0}</span>
                                       <span style={{color: '#ff8a65'}}>Conflict: {types.CONFLICT || 0}</span>
                                       <span style={{color: '#e57373'}}>Capacity: {types.CAPACITY || 0}</span>
                                   </div>
                               </div>
                           );
                        })}
                    </div>
                </div>
            </div>

            <div className="card">
                <h3>Latency Distribution</h3>
                <div style={{marginTop: '1rem'}}>
                    {Object.entries(histogram).map(([bucket, count]) => (
                        <div key={bucket} style={{marginBottom: '0.5rem'}}>
                            <div style={{display: 'flex', justifyContent: 'space-between', fontSize: '0.9rem', marginBottom: '0.2rem'}}>
                                <span>{bucket} cycles</span>
                                <span>{count}</span>
                            </div>
                            <div style={{
                                width: '100%',
                                height: '20px',
                                backgroundColor: '#333',
                                borderRadius: '4px',
                                overflow: 'hidden'
                            }}>
                                <div style={{
                                    height: '100%',
                                    width: `${maxCount > 0 ? (count / maxCount) * 100 : 0}%`,
                                    backgroundColor: '#7e57c2',
                                    transition: 'width 0.5s'
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
