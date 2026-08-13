# Visual Cache Simulator

A comprehensive, interactive memory hierarchy simulator that visualizes how data moves between L1, L2, L3 Caches, Main Memory (RAM), and Virtual Memory (TLB, Page Tables). Built with **C++17** for the core simulation engine, **Node.js** for the API middleware, and **React (Vite)** for a modern, responsive frontend.

## 🚀 Features

- **Visual Simulation**: Watch data move through the memory hierarchy in real-time with step-by-step execution.
- **Deep Decomposition**: Inspect Virtual Address translation (VPN → PPN) and Physical Address usage (Tag, Index, Offset).
- **Advanced Policies**: Select from **LRU**, **FIFO**, **LFU**, and **Optimal (Belady's Algorithm)** replacement policies.
- **Comparison Mode**: Benchmarks all 4 policies **in parallel** using `std::thread`; ~4x faster than running them sequentially.
- **Analytics Dashboard**: Detailed breakdown of Hit Rates, Miss Types (Cold, Conflict, Capacity), and Latency Histograms.
- **L1 Cache State Grid**: Live Set×Way visualisation of L1 cache contents at every simulation step — highlighted on access, coloured by state (clean/dirty/evicted).
- **Trace File Import**: Import real Valgrind lackey `.din` traces or plain `R/W` text files directly into the simulator.
- **Configurable Hardware**: Customise cache sizes, associativity, block sizes, and latencies for L1/L2/L3/RAM/Disk.
- **Unit Tests**: 20+ Google Test cases covering cache logic, all replacement policies, and virtual memory behaviour.

## 🛠 Tech Stack

| Layer | Technology |
|-------|-----------|
| Simulation Engine | C++17 · CMake · `std::thread` |
| API Middleware | Node.js · Express |
| Frontend | React 19 · Vite · Vanilla CSS |
| Testing | Google Test (via CMake `FetchContent`) |

## 📋 Prerequisites

- **Node.js** (v16+)
- **CMake** (3.14+)
- **C++ Compiler** with C++17 and threading support (MinGW-w64 / GCC / MSVC)

## ⚙️ Setup & Installation

### 1. Clone the Repository
```sh
git clone <repository-url>
cd cache-simulator
```

### 2. Build the C++ Backend (CRITICAL)
```cmd
cmake -S backend -B backend/build
cmake --build backend/build
```
> If you modify any C++ source, re-run `cmake --build backend/build`.

### 3. Install API Dependencies
```sh
cd api && npm install && cd ..
```

### 4. Install Frontend Dependencies
```sh
cd frontend && npm install && cd ..
```

## 🏃‍♂️ How to Run

Start the API and frontend in **two separate terminals**:

### Terminal 1 — API Server (port 3001)
```sh
cd api
node server.js
```

### Terminal 2 — Frontend (port 5173)
```sh
cd frontend
npm run dev
```

Open **http://localhost:5173** in your browser.

## 🧪 Running Unit Tests

After building the backend, run the full Google Test suite:
```cmd
cd backend/build
ctest --output-on-failure
```

Or run the test binary directly:
```cmd
backend/build/tests/run_tests.exe
```

Tests cover:
- `Cache` — cold miss, hit detection, eviction, dirty bit, `markDirty`, `isFull`
- `ReplacementPolicy` — LRU/FIFO/LFU/Optimal victim selection and metadata updates
- `MemoryHierarchy` — L1/L2 hit paths, TLB hit/miss, page fault, address translation

## 📂 Project Structure

```
cache-simulator/
├── backend/                  # C++ Simulation Engine
│   ├── src/
│   │   ├── main.cpp          # Entry point; --compare-all multithreaded mode
│   │   ├── MemoryHierarchy.cpp  # Core logic (Caches + VM)
│   │   └── ReplacementPolicy.cpp # LRU, FIFO, LFU, Optimal
│   ├── include/              # Header files
│   ├── tests/                # Google Test suite
│   │   ├── test_cache.cpp
│   │   ├── test_replacement.cpp
│   │   └── test_memory_hierarchy.cpp
│   └── CMakeLists.txt
├── frontend/                 # React Application
│   └── src/
│       ├── App.jsx           # Main layout, trace import, useCacheState hook
│       ├── CacheGrid.jsx     # L1 Set×Way state visualisation
│       ├── AnalyticsDashboard.jsx
│       └── ComparisonPage.jsx
├── api/
│   └── server.js             # /simulate and /compare (single --compare-all process)
└── README.md
```

## ⚡ Architecture: Multithreaded Policy Comparison

The `/compare` endpoint previously spawned **4 separate C++ processes** sequentially. It now spawns **one process** with the `--compare-all` flag. Inside C++:

```
Main thread: read config + trace once, preprocess next-use (O(N))
├── Thread 0: MemoryHierarchy (LRU)     → SimulationStats[0]
├── Thread 1: MemoryHierarchy (FIFO)    → SimulationStats[1]
├── Thread 2: MemoryHierarchy (LFU)     → SimulationStats[2]
└── Thread 3: MemoryHierarchy (Optimal) → SimulationStats[3]
join all → emit single JSON document
```

Each thread has its own independent `MemoryHierarchy` instance → **zero shared mutable state → no mutexes needed**.

## 🐛 Troubleshooting

- **Build Fails**: Ensure your compiler supports C++17. On Windows, MinGW-w64 8+ works well.
- **`memory_sim.exe` not found**: Confirm `cmake --build backend/build` succeeded; the binary lands at `backend/build/memory_sim.exe`.
- **API crashes with "spawn ENOENT"**: The binary is missing — rebuild the C++ backend.
- **Tests fail to configure**: CMake 3.14+ is required for `FetchContent`. An internet connection is needed on the first configure to download Google Test.
