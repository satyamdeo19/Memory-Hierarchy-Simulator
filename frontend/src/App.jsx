import { useState, useEffect, useRef } from 'react'
import ComparisonPage from './ComparisonPage';
import AnalyticsDashboard from './AnalyticsDashboard';

function App() {
  const [config, setConfig] = useState({
    l1Size: 1024,
    l1Assoc: 2,
    l2Size: 4096,
    l2Assoc: 4,
    l3Size: 16384,
    l3Assoc: 8,
    l3Latency: 20,
    ramSize: 1048576,
    ramAssoc: 16,
    diskLatency: 10000,
    blockSize: 64,
    addresses: "R 8000\nW 8000\nR 8200\nW 8400\nR 8000"
  });

  const [activeTab, setActiveTab] = useState('simulate');
  
  // Simulator State
  const [simulationResult, setSimulationResult] = useState(null);
  const [summary, setSummary] = useState(null); // NEW: Statistics Summary
  const [step, setStep] = useState(0); 
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);
  const [isPlaying, setIsPlaying] = useState(false);
  const playInterval = useRef(null);

  const handleRun = async () => {
    setLoading(true);
    setError(null);
    setIsPlaying(false);
    try {
      const addrList = config.addresses.split('\n').filter(s => s.trim() !== "");
      
      const response = await fetch('http://localhost:3001/simulate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          ...config,
          addresses: addrList
        })
      });
      
      const data = await response.json();
      if (data.error) throw new Error(data.error);
      
      // Handle new format { trace, summary } or old format [...]
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

  const nextStep = () => {
    if (simulationResult && step < simulationResult.length - 1) {
      setStep(s => s + 1);
    } else {
        setIsPlaying(false);
    }
  };

  const prevStep = () => {
    if (step > 0) setStep(s => s - 1);
  };
  
  const togglePlay = () => {
      setIsPlaying(!isPlaying);
  };
  
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
            if (e.key === 'ArrowLeft') prevStep();
            if (e.key === ' ') togglePlay(); 
        }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [step, simulationResult, isPlaying, activeTab]);

  const currentStepData = simulationResult ? simulationResult[step] : null;

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
      
      switch(res.miss_type) {
          case 'COLD': return "Cold Miss";
          case 'CONFLICT': return "Conflict Miss";
          case 'CAPACITY': return "Capacity Miss";
          default: return "Miss";
      }
  };

  const getEvictionInfo = (level) => {
      if (!currentStepData || !currentStepData.evictions) return null;
      const evicts = currentStepData.evictions.filter(e => e.level === level);
      if (evicts.length === 0) return null;
      
      return evicts.map((e, idx) => (
          <div key={idx} className="eviction-alert">
              Evicted Line<br/>
              Tag: {e.tag} (Set {e.set})<br/>
              Dirty: {e.dirty ? "YES" : "No"}<br/>
              {e.next_use_distance && <span style={{fontSize: '0.8em', color: '#ffd700'}}>Next Use: {e.next_use_distance}</span>}
          </div>
      ));
  };
  
  const getExplanationPanel = (level) => {
      const status = getLevelStatus(level);
      if (status === 'idle') return null;
      
      const res = getLevelResult(level);
      const evictions = getEvictionInfo(level);
      const missExpl = getMissExplanation(level);
      
      return (
          <div className="explanation-tooltip">
             <strong>{status.toUpperCase()}</strong>
             {res && res.latency > 0 && <div>+{res.latency} cycles</div>}
             {missExpl && <p>{missExpl}</p>}
             {evictions}
          </div>
      );
  };

  const exportAnalytics = () => {
      if (!summary) return;
      const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(summary, null, 2));
      const downloadAnchorNode = document.createElement('a');
      downloadAnchorNode.setAttribute("href", dataStr);
      downloadAnchorNode.setAttribute("download", "simulation_analytics.json");
      document.body.appendChild(downloadAnchorNode);
      downloadAnchorNode.click();
      downloadAnchorNode.remove();
  };

  return (
    <div className="container">
      <div className="header">
        <h1>Memory Hierarchy Simulator</h1>
        <div style={{display: 'flex', gap: '1rem'}}>
            <button 
                className={`nav-btn ${activeTab === 'simulate' ? 'active' : ''}`} 
                onClick={() => setActiveTab('simulate')}
                style={{backgroundColor: activeTab === 'simulate' ? '#29b6f6' : '#333'}}
            >
                Simulator
            </button>
            <button 
                className={`nav-btn ${activeTab === 'analytics' ? 'active' : ''}`} 
                onClick={() => setActiveTab('analytics')}
                style={{backgroundColor: activeTab === 'analytics' ? '#29b6f6' : '#333'}}
            >
                Analytics
            </button>
            <button 
                className={`nav-btn ${activeTab === 'compare' ? 'active' : ''}`} 
                onClick={() => setActiveTab('compare')}
                style={{backgroundColor: activeTab === 'compare' ? '#29b6f6' : '#333'}}
            >
                Compare Policies
            </button>
        </div>
      </div>

      <div className="controls">
        <div className="card">
          <h2>Configuration</h2>
          <div className="config-grid">
             <div className="input-group">
                <label>L1 Size</label>
                <input type="number" value={config.l1Size} onChange={e => setConfig({...config, l1Size: parseInt(e.target.value)})} />
             </div>
             <div className="input-group">
                <label>L1 Assoc</label>
                <input type="number" value={config.l1Assoc} onChange={e => setConfig({...config, l1Assoc: parseInt(e.target.value)})} />
             </div>
             <div className="input-group">
                <label>L2 Size</label>
                <input type="number" value={config.l2Size} onChange={e => setConfig({...config, l2Size: parseInt(e.target.value)})} />
             </div>
             <div className="input-group">
                <label>L2 Assoc</label>
                <input type="number" value={config.l2Assoc} onChange={e => setConfig({...config, l2Assoc: parseInt(e.target.value)})} />
             </div>
             <div className="input-group">
                <label>L3 Size</label>
                <input type="number" value={config.l3Size} onChange={e => setConfig({...config, l3Size: parseInt(e.target.value)})} />
             </div>
             <div className="input-group">
                <label>RAM Size</label>
                <input type="number" value={config.ramSize} onChange={e => setConfig({...config, ramSize: parseInt(e.target.value)})} />
             </div>
             <div className="input-group">
                <label>Disk Latency</label>
                <input type="number" value={config.diskLatency} onChange={e => setConfig({...config, diskLatency: parseInt(e.target.value)})} />
             </div>
             <div className="input-group">
                <label>Block Size</label>
                <input type="number" value={config.blockSize} onChange={e => setConfig({...config, blockSize: parseInt(e.target.value)})} />
             </div>
          </div>
          {activeTab === 'simulate' && (
              <>
                <button className="primary-btn" onClick={handleRun} disabled={loading}>
                    {loading ? 'Simulating...' : 'Run Simulation'}
                </button>
                {error && <div style={{color: 'red', marginTop: '1rem'}}>{error}</div>}
              </>
          )}
        </div>
        
        <div className="card">
           <h2>Access Trace</h2>
           <div className="input-group">
               <textarea 
                  style={{minHeight: '200px', fontSize: '1rem'}}
                  value={config.addresses} 
                  onChange={e => setConfig({...config, addresses: e.target.value})} 
                  placeholder="e.g. R 8000"
               />
           </div>
        </div>
      </div>

      {activeTab === 'compare' ? (
          <ComparisonPage config={config} />
      ) : activeTab === 'analytics' ? (
            simulationResult && summary ? (
                <>
                <div className="card" style={{textAlign: 'right'}}>
                    <button className="nav-btn" onClick={exportAnalytics}>Export JSON</button>
                </div>
                <AnalyticsDashboard summary={summary} />
                </>
            ) : <div className="card">Run a simulation first to see analytics.</div>
      ) : (
          simulationResult && currentStepData && (
            <>
                <div className="card">
                   <div style={{display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '2rem'}}>
                        <div>
                            <span className="latency-label">Operation</span>
                            <div style={{fontSize: '2rem', color: currentStepData.type === 'WRITE' ? '#ff4081' : '#40c4ff'}}>
                                {currentStepData.type} <span style={{color: '#fff'}}>0x{parseInt(currentStepData.address).toString(16).toUpperCase()}</span>
                            </div>
                        </div>
                        <div>
                            <span className="latency-label">Decomposition</span>
                            <div style={{fontFamily: 'monospace', fontSize: '1.2rem'}}>
                               Tag: {currentStepData.decomposition.tag} | Idx: {currentStepData.decomposition.index} | Off: {currentStepData.decomposition.offset}
                            </div>
                        </div>
                   </div>
                
                   <div className="visualization">
                      <div className={`mem-level ${getLevelStatus('L1')}`}>
                         <h3>L1</h3>
                         <div className="status-badge">{getLevelStatus('L1').toUpperCase()}</div>
                         {getExplanationPanel('L1')}
                      </div>
                      
                      <div className="arrow">→</div>
                      
                      <div className={`mem-level ${getLevelStatus('L2')}`}>
                         <h3>L2</h3>
                         <div className="status-badge">{getLevelStatus('L2') === 'idle' ? '-' : getLevelStatus('L2').toUpperCase()}</div>
                         {getExplanationPanel('L2')}
                      </div>
    
                      <div className="arrow">→</div>
    
                      <div className={`mem-level ${getLevelStatus('L3')}`}>
                         <h3>L3</h3>
                         <div className="status-badge">{getLevelStatus('L3') === 'idle' ? '-' : getLevelStatus('L3').toUpperCase()}</div>
                         {getExplanationPanel('L3')}
                      </div>
                      
                      <div className="arrow">→</div>
                      
                      <div className={`mem-level ${getLevelStatus('RAM')}`}>
                         <h3>RAM</h3>
                         <div className="status-badge">{getLevelStatus('RAM') === 'idle' ? '-' : getLevelStatus('RAM').toUpperCase()}</div>
                         {getExplanationPanel('RAM')}
                      </div>
                      
                      <div className="arrow">→</div>
                      
                      <div className={`mem-level ${getLevelStatus('DISK')}`}>
                         <h3>Disk</h3>
                         <div className="status-badge">{getLevelStatus('DISK') === 'idle' ? '-' : 'ACCESS'}</div>
                         {getLevelStatus('DISK') !== 'idle' && (
                             <div className="explanation-tooltip">
                                 Fetch from Disk<br/>
                                 +{config.diskLatency} cycles
                             </div>
                         )}
                      </div>
                   </div>
    
                   <div className="latency-display">
                      {currentStepData.total_latency} <span className="latency-label">Total Cycles</span>
                   </div>
                </div>
    
                <div className="nav-controls">
                    <button className="nav-btn" onClick={prevStep} disabled={step === 0}>← Previous</button>
                    <button className="nav-btn" onClick={togglePlay} style={{minWidth: '80px'}}>
                        {isPlaying ? 'Pause' : 'Play ▶'}
                    </button>
                    <button className="nav-btn" onClick={nextStep} disabled={step === simulationResult.length - 1}>Next →</button>
                </div>
                
                <div className="step-info">
                   Step {step + 1} of {simulationResult.length}
                </div>
            </>
          )
      )}
    </div>
  )
}

export default App
