# Visual Cache Simulator

A comprehensive, interactive memory hierarchy simulator that visualizes how data moves between L1, L2, L3 Caches, Main Memory (RAM), and Virtual Memory (TLB, Page Tables). Built with **C++17** for the core simulation engine and compiled to **WebAssembly (Wasm)** to run instantly in the browser alongside a modern, responsive **React (Vite)** frontend.

**🌐 Live Demo:** [https://satyamdeo19.github.io/Memory-Hierarchy-Simulator/](https://satyamdeo19.github.io/Memory-Hierarchy-Simulator/)

## 🚀 Features

- **Zero-Latency Serverless Architecture**: The C++ simulation engine runs natively in the browser via WebAssembly. No backend servers, no cold starts, and maximum privacy.
- **Visual Simulation**: Watch data move through the memory hierarchy in real-time with step-by-step execution.
- **Deep Decomposition**: Inspect Virtual Address translation (VPN → PPN) and Physical Address usage (Tag, Index, Offset).
- **Advanced Policies**: Select from **LRU**, **FIFO**, **LFU**, and **Optimal (Belady's Algorithm)** replacement policies.
- **Comparison Mode**: Benchmarks all 4 policies simultaneously to compare hit rates and total latencies.
- **Analytics Dashboard**: Detailed breakdown of Hit Rates, Miss Types (Cold, Conflict, Capacity), and Latency Histograms.
- **L1 Cache State Grid**: Live Set×Way visualisation of L1 cache contents at every simulation step — highlighted on access, coloured by state (clean/dirty/evicted).
- **Trace File Import**: Import real Valgrind lackey `.din` traces or plain `R/W` text files directly into the simulator.
- **Configurable Hardware**: Customise cache sizes, associativity, block sizes, and latencies for L1/L2/L3/RAM/Disk.

## 🛠 Tech Stack

| Layer | Technology |
|-------|-----------|
| Simulation Engine | C++17 · Emscripten (WebAssembly) |
| Frontend | React 19 · Vite · Vanilla CSS |
| CI/CD | GitHub Actions |
| Hosting | GitHub Pages |

## ⚙️ Setup & Installation (Local Development)

Because the project uses WebAssembly, the C++ code must be compiled using Emscripten before the React frontend can run.

### 1. Prerequisites
- **Node.js** (v18+)
- **Emscripten SDK (emsdk)** installed and activated in your terminal.

### 2. Clone the Repository
```sh
git clone https://github.com/satyamdeo19/Memory-Hierarchy-Simulator.git
cd Memory-Hierarchy-Simulator
```

### 3. Compile the C++ Engine to WebAssembly
Run the following Emscripten command from the root of the project to generate `simulator.js` and `simulator.wasm` inside the `frontend/public/` directory:
```sh
em++ backend/src/*.cpp -I backend/include -std=c++17 -o frontend/public/simulator.js -O3 --bind -s MODULARIZE=1 -s EXPORT_NAME="CacheSimulatorModule" -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" --no-entry
```

### 4. Run the React Frontend
```sh
cd frontend
npm install
npm run dev
```

Open **http://localhost:5173** in your browser.

## 🚀 Deployment (CI/CD)

This project is configured with a fully automated CI/CD pipeline using **GitHub Actions**.
Whenever you push to the `main` branch, the `.github/workflows/deploy.yml` script will automatically:
1. Setup the Emscripten SDK.
2. Compile the C++ engine to WebAssembly.
3. Build the React frontend using Vite.
4. Deploy the static assets to **GitHub Pages**.

## 📂 Project Structure

```
Memory-Hierarchy-Simulator/
├── backend/                  # C++ Simulation Engine
│   ├── src/
│   │   ├── main.cpp          # Entry point & Emscripten bindings
│   │   ├── MemoryHierarchy.cpp  # Core logic (Caches + VM)
│   │   └── ReplacementPolicy.cpp # LRU, FIFO, LFU, Optimal
│   └── include/              # Header files
├── frontend/                 # React Application
│   ├── public/               # Static assets & Compiled Wasm
│   └── src/
│       ├── App.jsx           # Main layout, trace import, Wasm loading
│       ├── CacheGrid.jsx     # L1 Set×Way state visualisation
│       ├── AnalyticsDashboard.jsx
│       └── ComparisonPage.jsx
└── .github/workflows/        # CI/CD deployment scripts
```

## ⚡ Architecture: Why WebAssembly?

Originally, this project utilized a Node.js Express backend to execute the C++ engine via child processes. While this worked, it introduced severe network latency and required backend hosting (which is prone to "cold starts" on free tiers). 

By migrating the core C++ engine to **WebAssembly**, we achieved a "Serverless" architecture. The C++ code is shipped directly to the client's browser, enabling instant, near-native execution speeds without any backend infrastructure. This eliminates server costs, removes network bottlenecks, and ensures user data never leaves their local machine.
