# Power Grid Simulator

An interactive power-grid simulation and visualiser built with **C++17** and **[Raylib](https://www.raylib.com/)**.

The grid is procedurally generated and evolves over a simulated year, combining realistic hourly (diurnal) and seasonal demand patterns. The engine automatically detects overloads and resolves them through load re-routing, demand throttling, or — as a last resort — node capacity upgrades.

---

## Features

| | |
|---|---|
| Procedural topology | Configurable power plants, substations, and consumers |
| Consumer types | Residential · Hospital · Institute · Industrial (distinct demand profiles) |
| Simulation | Full year (365 days × 24 hours) with cosine seasonal multiplier |
| Load balancing | Widest-path (max-bottleneck-capacity) re-routing algorithm |
| Fallbacks | Proportional demand throttling → manual node capacity upgrade |
| Visualisation | Zoomable/pannable camera with LOD, pulsing overload indicators, green fix-path overlay |

---

## Project structure

```
PowerGridSim/
├── include/
│   ├── globals.h          # Global demand constants, shared RNG, PairHash
│   ├── graph.h            # graph class declaration (all inner types & methods)
│   └── renderer.h         # Raylib draw-helper declarations
├── src/
│   ├── globals.cpp        # Definitions of global variables
│   ├── graph_core.cpp     # Node/edge constructors, topology generation
│   ├── graph_layout.cpp   # Spatial placement (BFS) + redundant connections
│   ├── graph_simulation.cpp  # Seasonal/diurnal demand model
│   ├── graph_overload.cpp    # Overload detection, re-routing, throttling, upgrades
│   ├── renderer.cpp       # DrawNodeA, DrawNodeB, DrawArrow
│   └── main.cpp           # Raylib window + game-loop state machine
└── CMakeLists.txt
```

---

## Building

```bash
git clone https://github.com/<your-username>/PowerGridSim.git
cd PowerGridSim
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/PowerGridSim      # Linux / macOS
build\PowerGridSim.exe    # Windows
```

If Raylib is not found on your system, CMake downloads and compiles it automatically via `FetchContent` (internet required on first build).

**Requirements:** C++17 compiler · CMake ≥ 3.16 · Raylib 5.x (or auto-fetched)

---

## Controls

| Input | Action |
|---|---|
| Scroll wheel | Zoom in / out |
| Left-click drag | Pan camera |
| Left-click node / edge | Select and inspect |
| **I** | Toggle 1-second auto-loop |
| **U** | Upgrade selected overloaded substation |
| **C** | Clear all visual highlights |
| **Backspace** | Return to main menu |

---

## Bugs fixed during refactoring

| # | Location | Problem | Fix |
|---|---|---|---|
| 1 | `Vertex_B::demand` | Typed as `int`; simulation applies fractional kW deltas every hour, causing silent truncation | Changed to `float` |
| 2 | `Edge(Vertex_A*, Vertex_B*)` | `loss` field left uninitialised (UB) | Explicitly initialised to a random value in [0.5, 1.0] kW |
| 3 | `add_realistic_connections` | Max-heap never capped at `k_nearest`; could grow to O(N) | Heap is now correctly limited to `k_nearest` entries |
| 4 | `distributor` | Could produce zero-consumer substations for tight distributions | Each bin is now guaranteed ≥ 1 consumer |
| 5 | Input validation (`main`) | Allowed `num_substation == 0`, causing divide-by-zero in `distributor` | Validation changed to require all fields > 0 |
| 6 | `#include <bits/stdc++.h>` | Non-portable GCC extension | Replaced with explicit, portable headers throughout |
| 7 | `using namespace std;` | Polluted global namespace in translation units | Removed; fully-qualified names used instead |
