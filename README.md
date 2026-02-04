# Visual Cache Simulator

A comprehensive, interactive memory hierarchy simulator that visualizes how data moves between L1, L2, L3 Caches, and Main Memory (RAM), including Virtual Memory (TLB, Page Tables). Built with **C++** for the core simulation engine, **Node.js** for the API middleware, and **React (Vite)** for a modern, responsive frontend.

## 🚀 Features

- **Visual Simulation**: Watch data move through the memory hierarchy in real-time with step-by-step execution.
- **Deep Decomposition**: Inspect Virtual Address translation (VPN -> PPN) and Physical Address usage (Tag, Index, Offset).
- **Advanced Policies**: Select from **LRU**, **FIFO**, **LFU**, and **Optimal (Belady's Algorithm)** replacement policies.
- **Comparison Mode**: Run side-by-side benchmarks to compare how different policies perform on the same trace.
- **Analytics Dashboard**: detailed breakdown of Hit Rates, Miss Types (Cold, Conflict, Capacity), and Latency Histograms.
- **Configurable Hardware**: Customize cache sizes, associativity, block sizes, and latencies for L1/L2/L3/RAM/Disk.

## 🛠 Tech Stack

- **Backend**: C++17 (CMake) - High-performance core simulation logic.
- **API**: Node.js (Express) - Spawns the C++ process, parses stdout (JSON), and serves metrics to the frontend.
- **Frontend**: React 19 + Vite + Vanilla CSS - Dynamic UI for visualization and charts.

## 📋 Prerequisites

Ensure you have the following installed:
- **Node.js** (v16+)
- **CMake** (3.12+)
- **C++ Compiler** (MinGW-w64 with GCC/G++ or MSVC)

## ⚙️ Setup & Installation

### 1. Clone the Repository
```sh
git clone <repository-url>
cd cache-simulator
```

### 2. Build the C++ Backend (CRITICAL)
You must compile the C++ simulator before running the app.
```cmd
cmake -S backend -B backend/build
cmake --build backend/build
```
*Note: If you make changes to the C++ code, you must run `cmake --build backend/build` again to update the executable.*

### 3. Install API Dependencies
```sh
cd api
npm install
cd ..
```

### 4. Install Frontend Dependencies
```sh
cd frontend
npm install
cd ..
```

## 🏃‍♂️ How to Run

To run the full application, you need to start the API and Frontend servers in separate terminals.

### Terminal 1: API Server
The API listens on port **3001**.
```sh
cd api
node server.js
```
*(Use `node server.js` to ensure you are running the latest version logic).*

### Terminal 2: Frontend
The frontend runs on port **5173**.
```sh
cd frontend
npm run dev
```

Open **http://localhost:5173** in your browser.

## 📂 Project Structure

```
cache-simulator/
├── backend/                # C++ Simulation Engine
│   ├── src/
│   │   ├── main.cpp        # Entry Point & JSON Output formatting
│   │   ├── MemoryHierarchy # Core Logic (Caches + VM)
│   │   └── ReplacementPolicy # LRU, FIFO, LFU, Optimal Logic
│   ├── include/            # Header Files
│   └── CMakeLists.txt      # Build Configuration
├── frontend/               # React Application
│   ├── src/                
│   │   ├── App.jsx         # Main Layout & Simulator Tab
│   │   ├── AnalyticsDashboard.jsx # Stats & Charts
│   │   └── ComparisonPage.jsx     # Policy Benchmarking
│   └── package.json
├── api/                    # Node.js Intermediate Server
│   ├── server.js           # /simulate and /compare endpoints
│   └── package.json
└── README.md               # This file
```

## 🐛 Troubleshooting

- **Backend Build Fails**: Ensure your C++ compiler supports C++11/14/17 features. If you see errors about "structured bindings" or "std::pair", ensure you are using a compatible compiler or that the code has been patched (we recently patched `main.cpp` for broader compatibility).
- **Executable Not Found**: The server expects the executable at `backend/build/memory_sim.exe`. Verify the build command succeeded.
- **API Errors**: If the API crashes with "spawn ENOENT", check that `memory_sim.exe` exists. If it crashes with "No JSON output", try simpler traces or check the backend capability.
