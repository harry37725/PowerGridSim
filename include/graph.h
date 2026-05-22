#pragma once
#include "globals.h"

#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <limits>

// Forward-declare Raylib Color so graph.h is independent of raylib.h.
// (graph itself never calls draw functions.)
struct Color;

// ===========================================================================
//  graph
//  Owns the entire power-grid topology:
//    Vertex_A  – power plants and substations
//    Vertex_B  – consumer nodes (residential / hospital / institute / industrial)
//    Edge      – transmission / distribution lines
// ===========================================================================
class graph {
public:

    // -----------------------------------------------------------------------
    // Inner node / edge types
    // -----------------------------------------------------------------------

    // Power-plant OR substation node
    struct Vertex_A {
        std::string node_type;
        float max_limit                 = 0.f;
        float current_downstream_demand = 0.f;
        int   x = 0, y = 0;

        explicit Vertex_A(std::string t) : node_type(std::move(t)) {}
    };

    // Consumer node
    struct Vertex_B {
        std::string node_type;
        float demand = 0.f;   // float so simulation can apply fractional deltas
        int   x = 0, y = 0;

        explicit Vertex_B(std::string t);   // defined in graph_core.cpp
    };

    // Transmission / distribution edge
    struct Edge {
        float loss         = 0.f;
        float max_load     = 0.f;
        float current_load = 0.f;

        Edge(Vertex_A* from, Vertex_A* to);   // plant→sub or sub→sub
        Edge(Vertex_A* from, Vertex_B* to);   // sub→consumer
    };

    // -----------------------------------------------------------------------
    // Overload bookkeeping
    // -----------------------------------------------------------------------
    struct OverloadInfo {
        float     ratio;    // current_load / max_load (higher = more severe)
        Vertex_A* source;
        Vertex_A* target;
        Edge*     edge;
        bool operator>(const OverloadInfo& o) const { return ratio > o.ratio; }
    };

    // -----------------------------------------------------------------------
    // Adjacency lists (public so the renderer can iterate them directly)
    // -----------------------------------------------------------------------
    std::unordered_map<Vertex_A*, std::vector<std::pair<Vertex_B*, Edge*>>> adj_substion_consumer;
    std::unordered_map<Vertex_A*, std::vector<std::pair<Vertex_A*, Edge*>>> adj_power_substation;
    std::unordered_map<Vertex_A*, std::vector<std::pair<Vertex_A*, Edge*>>> adj_reverse_power;

    // -----------------------------------------------------------------------
    // Runtime visualisation state (read by main/renderer each frame)
    // -----------------------------------------------------------------------
    std::unordered_set<Edge*>              overloaded_edges;
    std::vector<OverloadInfo>              pending_problems;
    Edge*                                  edge_being_fixed = nullptr;
    std::unordered_map<Vertex_A*, float>   nodes_to_throttle;
    std::unordered_set<Vertex_A*>          node_overloads_visual;
    std::unordered_set<Vertex_A*>          throttled_nodes_visual;
    std::vector<std::pair<Vertex_A*, Edge*>> fix_path;
    Vertex_B*                              last_demand_increase_consumer = nullptr;

    // Simulation clock
    int sim_hour = 0;   // 0-23
    int sim_day  = 0;   // 0-364

    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------

    // Build topology from user-supplied counts.
    void mapping(int powerplant_count, int substation_count, int consumer_count);

    // Assign world-space (x,y) positions to every node via BFS.
    std::vector<std::pair<std::string, std::pair<int,int>>> layout_graph();

    // Add up to k_nearest redundant substation↔substation backup links.
    void add_realistic_connections(int k_nearest = 2);

    // --- Simulation helpers ---
    int         total_hours_elapsed() const { return sim_day * 24 + sim_hour; }
    float       seasonal_factor(int day) const;
    std::string get_month_name(int day) const;
    int         get_month_number(int day) const;
    float       diurnal_factor(const std::string& type, int hour) const;
    float       target_demand_at_hour(const Vertex_B* c, int hour, int day) const;

    // Advance simulation one hour; returns consumer with largest |delta|.
    Vertex_B* advance_hour_demand();

    // --- Overload detection & resolution ---
    bool detect_overloads();          // Stage 1: mark problems
    void apply_overload_fixes();      // Stage 2/3: reroute or throttle
    void overloading_edge();          // Legacy combined wrapper
    void apply_demand_reduction_updates();  // Call every frame
    void check_node_overloads();
    void upgrade_selected_node_limit(Vertex_A* node);

private:

    // --- Layout helpers ---
    using GridKey = std::pair<int,int>;

    double GetDistance(int x1, int y1, int x2, int y2) const;
    int    GetRandom(int lo, int hi) const;

    bool is_position_ok_A(Vertex_A* node, int x, int y,
        const std::unordered_map<GridKey, std::vector<Vertex_A*>, PairHash>& grid,
        int cell_size) const;
    bool is_position_ok_B(Vertex_B* node, int x, int y,
        const std::unordered_map<GridKey, std::vector<Vertex_B*>, PairHash>& grid,
        int cell_size) const;

    bool  DoesEdgeExist(Vertex_A* a, Vertex_A* b) const;
    Edge* find_edge(Vertex_A* from, Vertex_A* to) const;

    // --- Topology-building helpers ---
    std::vector<int> distributor(int total_count, int divide_count);
    void substation_consumer_connector(Vertex_A* sub, int consumer_count);
    void map_powerplants_to_substations(std::vector<Vertex_A*>& plants,
                                        std::vector<Vertex_A*>& subs);

    // --- Widest-path (max-bottleneck-capacity) search ---
    struct PathState {
        float min_available_capacity;
        std::vector<std::pair<Vertex_A*, Edge*>> path;

        PathState(float cap, std::vector<std::pair<Vertex_A*, Edge*>> p)
            : min_available_capacity(cap), path(std::move(p)) {}

        // Max-heap: highest capacity has priority
        bool operator<(const PathState& o) const {
            return min_available_capacity < o.min_available_capacity;
        }
    };

    std::pair<float, std::vector<std::pair<Vertex_A*, Edge*>>>
    find_highest_capacity_path(Vertex_A* source, Vertex_A* target);

    void try_add_capacity_path(
        const PathState& cur, Vertex_A* next, Edge* edge,
        std::priority_queue<PathState>& pq,
        std::unordered_map<Vertex_A*, float>& cap_map);

    // --- Demand / capacity propagation ---
    void propagate_demand_change(Vertex_A* node, float delta,
                                 std::unordered_set<Vertex_A*>& visited);
    void propagate_limit_increase(Vertex_A* node, float increase,
                                  std::unordered_set<Vertex_A*>& visited);

    // --- Overload resolution (internal) ---
    bool fix_overloading_problem(Vertex_A* source, Vertex_A* target,
                                 Edge* overloaded_edge, bool visualize_this_fix);
};
