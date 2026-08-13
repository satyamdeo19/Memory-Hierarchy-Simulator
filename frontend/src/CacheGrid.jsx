import React from 'react';

/**
 * CacheGrid — renders an L1 Set × Way state table.
 *
 * Props:
 *   cacheState  : Array<Array<{valid, tag, dirty}>>  — [set][way]
 *   numSets     : number
 *   associativity: number
 *   activeSet   : number | null   — highlighted set index for current access
 *   activeTag   : string | null   — tag of the block just accessed
 *   evictedTag  : string | null   — tag that was just evicted (shown in red)
 */
function CacheGrid({ cacheState, numSets, associativity, activeSet, activeTag, evictedTag }) {
    if (!cacheState || cacheState.length === 0) return null;

    return (
        <div className="cache-grid-wrapper">
            <div className="cache-grid-header">
                <h3 className="cache-grid-title">L1 Cache State</h3>
                <div className="cache-grid-legend">
                    <span className="legend-item legend-hit">Hit / Clean</span>
                    <span className="legend-item legend-dirty">Dirty</span>
                    <span className="legend-item legend-evict">Evicted</span>
                    <span className="legend-item legend-empty">Empty</span>
                </div>
            </div>

            <div className="cache-grid-scroll">
                <table className="cache-grid-table">
                    <thead>
                        <tr>
                            <th className="cache-set-label-th">Set</th>
                            {Array.from({ length: associativity }, (_, w) => (
                                <th key={w} className="cache-way-th">Way {w}</th>
                            ))}
                        </tr>
                    </thead>
                    <tbody>
                        {cacheState.map((ways, setIdx) => {
                            const isActiveSet = setIdx === activeSet;
                            return (
                                <tr
                                    key={setIdx}
                                    className={`cache-set-row${isActiveSet ? ' active-set' : ''}`}
                                >
                                    <td className="cache-set-label">
                                        {isActiveSet
                                            ? <span className="active-set-badge">▶ {setIdx}</span>
                                            : setIdx}
                                    </td>
                                    {ways.map((line, wayIdx) => {
                                        let cellClass = 'cache-cell';
                                        let label = '—';

                                        if (line.valid) {
                                            const tagNum = parseInt(line.tag, 16);
                                            const activeTagNum = activeTag ? parseInt(activeTag, 16) : -1;
                                            const evictedTagNum = evictedTag ? parseInt(evictedTag, 16) : -2;

                                            label = `0x${tagNum.toString(16).toUpperCase()}`;

                                            if (isActiveSet && tagNum === evictedTagNum) {
                                                cellClass += ' cell-evicted';
                                            } else if (isActiveSet && tagNum === activeTagNum) {
                                                cellClass += ' cell-active';
                                            } else if (line.dirty) {
                                                cellClass += ' cell-dirty';
                                            } else {
                                                cellClass += ' cell-valid';
                                            }
                                        } else {
                                            cellClass += ' cell-empty';
                                        }

                                        return (
                                            <td key={wayIdx} className={cellClass}>
                                                <div className="cell-tag">{label}</div>
                                                {line.valid && line.dirty && (
                                                    <div className="cell-dirty-badge">D</div>
                                                )}
                                            </td>
                                        );
                                    })}
                                </tr>
                            );
                        })}
                    </tbody>
                </table>
            </div>
        </div>
    );
}

export default CacheGrid;
