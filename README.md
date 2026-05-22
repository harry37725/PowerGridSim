<div align="center">

# ⚡ Power Grid Simulator

**A real-time interactive power-grid simulation and visualiser built in C++17 with Raylib.**

Procedurally generate electricity networks, watch demand shift across a full simulated year, and let the engine automatically detect and fix overloads — or intervene yourself.


[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus)](https://en.cppreference.com/w/cpp/17)
[![Raylib](https://img.shields.io/badge/Raylib-5.0-orange)](https://www.raylib.com/)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-green?logo=cmake)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)]()

</div>

---

## Table of Contents

- [Overview](#overview)
- [Screenshots](#screenshots)
- [Features](#features)
- [How It Works](#how-it-works)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Controls](#controls)
- [Simulation Model](#simulation-model)
- [Overload Resolution System](#overload-resolution-system)
- [Technical Details](#technical-details)
- [Bugs Fixed](#bugs-fixed)
- [Contributing](#contributing)

---

## Overview

Power Grid Simulator generates a realistic electricity distribution network from three simple parameters — number of power plants, substations, and consumers. The network then evolves hour-by-hour across a full 365-day year, with each consumer type following its own diurnal (time-of-day) and seasonal demand curve.

When a line or node becomes overloaded, the engine responds automatically in three escalating stages:

1. **Re-route** load via the widest available alternative path (max-bottleneck-capacity algorithm)
2. **Throttle** consumer demand at overloaded substations proportionally
3. **Upgrade** node capacity as a last resort

Everything is visualised in real time with a zoomable, pannable 2-D camera and colour-coded overlays.

---

## Features

### Network Generation
- Configurable number of **power plants**, **substations**, and **consumer nodes**
- 40 % of substations connect directly to power plants; 60 % form a tree through other substations
- **k-nearest-neighbour backup links** added after layout for redundancy
- Spatial placement via BFS with collision avoidance using a spatial hash grid

### Consumer Types
| Type | Base Demand | Peak Hours | Notes |
|---|---|---|---|
| 🏠 Residential | 25 kW | Morning & evening | Two-peak diurnal profile |
| 🏥 Hospital | 90 kW | Around noon | Very flat profile — always on |
| 🏫 Institute | 15 kW | School hours (6–19) | Near-zero at night |
| 🏭 Industrial | 80 kW | Business hours | High base, peak at 14:00 |

### Demand Simulation
- Full **365-day × 24-hour** simulation clock
- **Cosine seasonal multiplier** — winter peak ×1.3, summer trough ×0.7
- **Per-type diurnal curves** using sinusoidal profiles
- Demand propagates proportionally up the network every tick

### Overload Resolution
- **Stage 1** — Widest-path (max bottleneck capacity) re-routing
- **Stage 2** — Proportional demand throttling at the affected substation
- **Stage 3** — Minimal node capacity upgrade (deficit + 5 % buffer)

### Visualisation
- Zoomable / pannable 2D camera with **three LOD levels**
  - Zoom < 0.5× → power plants only
  - Zoom ≥ 0.5× → substations + transmission lines
  - Zoom ≥ 1.0× → consumer nodes + distribution edges
- **Directional arrows** on every edge showing power-flow direction
- Pulsing **red arrows** on overloaded edges
- **Orange** highlight on the edge being fixed
- **Green** highlight on the chosen reroute path
- **Black** circle on overloaded substation nodes
- **White** circle on throttled substation nodes
- Pulsing **yellow ring** on the consumer with the largest demand change each tick
- Real-time inspector panel for selected nodes and edges

---

## How It Works

```
┌─────────────────────────────────────────────────────────┐
│                    USER INPUT SCREEN                    │
│          Power Plants │ Substations │ Consumers         │
└─────────────────┬───────────────────────────────────────┘
                  │ mapping()
                  ▼
┌─────────────────────────────────────────────────────────┐
│                  TOPOLOGY GENERATION                    │
│  distributor() → substation_consumer_connector()        │
│  map_powerplants_to_substations()                       │
│  layout_graph() BFS placement                           │
│  add_realistic_connections() k-NN backup links          │
└─────────────────┬───────────────────────────────────────┘
                  │ Every 1 second (auto-loop)
                  ▼
┌─────────────────────────────────────────────────────────┐
│               DEMAND SIMULATION TICK                    │
│  advance_hour_demand()                                  │
│  target_demand_at_hour() = diurnal × seasonal           │
│  propagate_demand_change() up the network               │
└─────────────────┬───────────────────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────────────────────┐
│              OVERLOAD DETECTION & FIX                   │
│  detect_overloads() → pending_problems[]                │
│       ↓ 0.5 s red highlight                             │
│  apply_overload_fixes()                                 │
│    ├─ find_highest_capacity_path() → reroute            │
│    ├─ apply_demand_reduction_updates() → throttle       │
│    └─ upgrade_selected_node_limit() → upgrade           │
└─────────────────────────────────────────────────────────┘
```

---

## Project Structure

```
PowerGridSim/
│
├── include/                        # Header files
│   ├── globals.h                   # Demand constants, shared RNG, PairHash
│   ├── graph.h                     # graph class — all inner types & method declarations
│   └── renderer.h                  # Raylib draw-helper declarations
│
├── src/                            # Implementation files
│   ├── globals.cpp                 # Global variable definitions
│   ├── graph_core.cpp              # Vertex/Edge constructors, topology building
│   ├── graph_layout.cpp            # BFS spatial placement + backup connections
│   ├── graph_simulation.cpp        # Seasonal/diurnal demand model
│   ├── graph_overload.cpp          # Overload detection, rerouting, throttling, upgrades
│   ├── renderer.cpp                # DrawNodeA, DrawNodeB, DrawArrow
│   └── main.cpp                    # Raylib window + game-loop state machine
│
├── assets/
│   └── screenshots/                # Screenshots for this README
│
├── CMakeLists.txt                  # Build configuration
├── .gitignore
└── README.md
```

---

## Getting Started

### Prerequisites

| Tool | Minimum Version |
|---|---|
| C++ Compiler (GCC / Clang / MSVC) | C++17 support |
| CMake | 3.16 |
| Raylib | 5.0 (auto-fetched if missing) |

### Build

```bash
# 1. Clone the repository
git clone https://github.com/<your-username>/PowerGridSim.git
cd PowerGridSim

# 2. Configure (Raylib is downloaded automatically if not found)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Compile
cmake --build build

# 4. Run
./build/PowerGridSim          # Linux / macOS
.\build\PowerGridSim.exe      # Windows
```

> **Note:** The first build may take a few minutes if CMake needs to download and compile Raylib via `FetchContent`. Subsequent builds are fast.

### Windows (Visual Studio)

```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

---

## Controls

| Input | Action |
|---|---|
| `Scroll Wheel` | Zoom in / out |
| `Left-click + Drag` | Pan the camera |
| `Left-click` on node/edge | Select and open the inspector panel |
| `I` | Toggle auto-loop (advances simulation 1 hour per second) |
| `U` | Upgrade the capacity of the selected overloaded substation |
| `C` | Clear all visual overload highlights |
| `Backspace` | Return to the setup screen |

---

## Simulation Model

### Seasonal Factor

The seasonal multiplier follows a cosine wave over the 365-day year:

```
seasonal(day) = 1.0 + 0.3 × cos(2π × day / 365)
```

- Day 0 (January) → multiplier ≈ **1.3** (winter peak)
- Day 182 (July) → multiplier ≈ **0.7** (summer trough)

### Diurnal Profiles

Each consumer type has its own hourly demand profile:

| Type | Formula | Peak |
|---|---|---|
| Residential | 0.4 × sin(morning) + sin(evening) | 08:00 and 20:00 |
| Hospital | 0.2 × sin(h) | 12:00 (very flat) |
| Institute | sin(h), returns −0.8 outside hours 6–19 | 11:00 |
| Industrial | sin(h) | 14:00 |

### Combined Demand

```
demand(consumer, hour, day) = max(5 kW, (base + amp × diurnal(hour)) × seasonal(day))
```

The 5 kW floor prevents any consumer from going completely dark.

---

## Overload Resolution System

Overloads are detected and resolved in a **staged, visualised pipeline**:

### Stage 1 — Detection (`detect_overloads`)
Scans every edge and node. An overload is flagged when:
- **Edge ratio** ≥ 75 % (`current_load / max_load ≥ 0.75`)
- **Node demand** > `max_limit`

All problems are sorted by severity (most overloaded first).

### Stage 2a — Rerouting (`fix_overloading_problem`)
For each overloaded edge, runs a **widest-path search** (modified Dijkstra maximising bottleneck capacity) to find the best alternative route:
- Path must handle ≥ 20 % of the required relief
- Path must not push any of its own edges above 75 % after the transfer
- On success: load is shifted, edges flash **green**

### Stage 2b — Throttling (`apply_demand_reduction_updates`)
If no viable reroute exists, consumer demand at the affected substation is **proportionally reduced** until the overload clears. Throttled substations flash **white**.

### Stage 3 — Upgrade (`upgrade_selected_node_limit`)
If throttling still isn't enough, the overloaded node's capacity is increased by exactly `deficit × 1.05` (just enough plus a 5 % buffer). This also propagates upward through all parent nodes and edges via `propagate_limit_increase`.

---

## Technical Details

| Aspect | Implementation |
|---|---|
| Language | C++17 |
| Graphics | Raylib 5.0 |
| Graph representation | Three adjacency maps (`adj_power_substation`, `adj_reverse_power`, `adj_substion_consumer`) |
| Shortest path | Modified Dijkstra (max-bottleneck / widest path) |
| Spatial layout | BFS from power plants with spatial hash-grid collision avoidance |
| Backup links | k-nearest-neighbour (k=2 by default) using a max-heap |
| Demand propagation | Recursive, proportional by edge `max_load`, with cycle detection via visited set |
| RNG | `std::mt19937` seeded from `std::random_device` |
| Build system | CMake 3.16 with FetchContent fallback for Raylib |

---


## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss what you'd like to change.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/my-feature`)
3. Commit your changes (`git commit -m 'Add my feature'`)
4. Push to the branch (`git push origin feature/my-feature`)
5. Open a Pull Request

---
