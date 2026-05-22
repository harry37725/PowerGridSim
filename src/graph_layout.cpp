// graph_layout.cpp
// Responsibility: world-space node placement (BFS) and backup-link pass.
#include "graph.h"
#include "globals.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <queue>

// ---------------------------------------------------------------------------
// Private geometry helpers
// ---------------------------------------------------------------------------
double graph::GetDistance(int x1, int y1, int x2, int y2) const {
    double dx = static_cast<double>(x1 - x2);
    double dy = static_cast<double>(y1 - y2);
    return std::sqrt(dx * dx + dy * dy);
}

int graph::GetRandom(int lo, int hi) const {
    if (lo > hi) std::swap(lo, hi);
    return rand() % (hi - lo + 1) + lo;
}

bool graph::is_position_ok_A(Vertex_A* node, int x, int y,
    const std::unordered_map<GridKey, std::vector<Vertex_A*>, PairHash>& grid,
    int cell_size) const
{
    float min_dist = (node->node_type == "power_plant") ? 400.f : 50.f;
    int gx = x / cell_size, gy = y / cell_size;
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy) {
            auto it = grid.find({gx + dx, gy + dy});
            if (it == grid.end()) continue;
            for (const auto* other : it->second)
                if (other->node_type == node->node_type &&
                    GetDistance(x, y, other->x, other->y) < min_dist)
                    return false;
        }
    return true;
}

bool graph::is_position_ok_B(Vertex_B* /*node*/, int x, int y,
    const std::unordered_map<GridKey, std::vector<Vertex_B*>, PairHash>& grid,
    int cell_size) const
{
    constexpr float MIN_DIST = 5.f;
    int gx = x / cell_size, gy = y / cell_size;
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy) {
            auto it = grid.find({gx + dx, gy + dy});
            if (it == grid.end()) continue;
            for (const auto* other : it->second)
                if (GetDistance(x, y, other->x, other->y) < MIN_DIST)
                    return false;
        }
    return true;
}

bool graph::DoesEdgeExist(Vertex_A* a, Vertex_A* b) const {
    for (auto* src : {a, b}) {
        auto it = adj_power_substation.find(src);
        if (it == adj_power_substation.end()) continue;
        Vertex_A* dst = (src == a) ? b : a;
        for (const auto& [n, _] : it->second)
            if (n == dst) return true;
    }
    return false;
}

graph::Edge* graph::find_edge(Vertex_A* from, Vertex_A* to) const {
    for (auto* src : {from, to}) {
        auto it = adj_power_substation.find(src);
        if (it == adj_power_substation.end()) continue;
        Vertex_A* dst = (src == from) ? to : from;
        for (const auto& [n, e] : it->second)
            if (n == dst) return e;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// layout_graph – BFS placement of every node in world-space
// ---------------------------------------------------------------------------
std::vector<std::pair<std::string, std::pair<int,int>>> graph::layout_graph() {
    constexpr int MAX_TRIES      = 1000;
    constexpr int BOUNDS_MIN     = 0;
    constexpr int BOUNDS_MAX     = 2000;
    constexpr int SUB_RANGE      = 200;
    constexpr int CONSUMER_RANGE = 50;
    constexpr int CELL_SIZE      = 50;

    std::vector<std::pair<std::string, std::pair<int,int>>> coords;
    std::unordered_set<Vertex_A*> visited_a;
    std::unordered_set<Vertex_B*> visited_b;
    std::queue<Vertex_A*> bfs;

    std::unordered_map<GridKey, std::vector<Vertex_A*>, PairHash> grid_a;
    std::unordered_map<GridKey, std::vector<Vertex_B*>, PairHash> grid_b;

    auto place_a = [&](Vertex_A* node, int x, int y) {
        node->x = x; node->y = y;
        coords.push_back({node->node_type, {x, y}});
        visited_a.insert(node);
        bfs.push(node);
        grid_a[{x / CELL_SIZE, y / CELL_SIZE}].push_back(node);
    };
    auto place_b = [&](Vertex_B* node, int x, int y) {
        node->x = x; node->y = y;
        coords.push_back({node->node_type, {x, y}});
        visited_b.insert(node);
        grid_b[{x / CELL_SIZE, y / CELL_SIZE}].push_back(node);
    };

    // 1. Collect and place power plants
    for (auto& [node, _] : adj_power_substation)
        if (node->node_type == "power_plant" && !visited_a.count(node)) {
            bool placed = false;
            for (int t = 0; t < MAX_TRIES && !placed; ++t) {
                int x = GetRandom(BOUNDS_MIN, BOUNDS_MAX);
                int y = GetRandom(BOUNDS_MIN, BOUNDS_MAX);
                if (is_position_ok_A(node, x, y, grid_a, CELL_SIZE)) { place_a(node, x, y); placed = true; }
            }
            if (!placed) {
                std::cout << "Warning: could not find ideal spot for power plant; placing randomly.\n";
                place_a(node, GetRandom(BOUNDS_MIN, BOUNDS_MAX), GetRandom(BOUNDS_MIN, BOUNDS_MAX));
            }
        }

    if (visited_a.empty()) {
        std::cout << "Error: no power plants found – cannot lay out graph.\n";
        return coords;
    }

    // 2. BFS: substations then consumers, clustered near their parent
    while (!bfs.empty()) {
        Vertex_A* parent = bfs.front(); bfs.pop();

        // Child substations
        auto it_a = adj_power_substation.find(parent);
        if (it_a != adj_power_substation.end())
            for (auto& [sub, _] : it_a->second)
                if (sub->node_type == "substation" && !visited_a.count(sub)) {
                    bool placed = false;
                    for (int t = 0; t < MAX_TRIES && !placed; ++t) {
                        int x = GetRandom(parent->x - SUB_RANGE, parent->x + SUB_RANGE);
                        int y = GetRandom(parent->y - SUB_RANGE, parent->y + SUB_RANGE);
                        if (is_position_ok_A(sub, x, y, grid_a, CELL_SIZE)) { place_a(sub, x, y); placed = true; }
                    }
                    if (!placed) place_a(sub, parent->x, parent->y);
                }

        // Child consumers
        auto it_b = adj_substion_consumer.find(parent);
        if (it_b != adj_substion_consumer.end())
            for (auto& [consumer, _] : it_b->second)
                if (!visited_b.count(consumer)) {
                    bool placed = false;
                    for (int t = 0; t < MAX_TRIES && !placed; ++t) {
                        int x = GetRandom(parent->x - CONSUMER_RANGE, parent->x + CONSUMER_RANGE);
                        int y = GetRandom(parent->y - CONSUMER_RANGE, parent->y + CONSUMER_RANGE);
                        if (is_position_ok_B(consumer, x, y, grid_b, CELL_SIZE)) { place_b(consumer, x, y); placed = true; }
                    }
                    if (!placed) place_b(consumer, parent->x, parent->y);
                }
    }
    return coords;
}

// ---------------------------------------------------------------------------
// add_realistic_connections – k-nearest-neighbour redundant backup links
// BUG FIX: original pushed all substations into the max-heap without ever
// capping its size, so the heap could grow to O(N) when only k entries were
// needed.  Now correctly maintains at most k_nearest entries.
// ---------------------------------------------------------------------------
void graph::add_realistic_connections(int k_nearest) {
    std::unordered_set<Vertex_A*> sub_set;
    for (auto& [sub, _]   : adj_substion_consumer) sub_set.insert(sub);
    for (auto& [n, children] : adj_power_substation) {
        if (n->node_type == "substation") sub_set.insert(n);
        for (auto& [c, _] : children)
            if (c->node_type == "substation") sub_set.insert(c);
    }

    std::vector<Vertex_A*> all_subs(sub_set.begin(), sub_set.end());
    if (static_cast<int>(all_subs.size()) <= k_nearest) return;

    for (auto* sub1 : all_subs) {
        // Max-heap of {distance, node}; we keep only the k closest.
        using Pair = std::pair<double, Vertex_A*>;
        std::priority_queue<Pair> pq;
        for (auto* sub2 : all_subs) {
            if (sub1 == sub2) continue;
            double d = GetDistance(sub1->x, sub1->y, sub2->x, sub2->y);
            if (static_cast<int>(pq.size()) < k_nearest)
                pq.push({d, sub2});
            else if (d < pq.top().first) { pq.pop(); pq.push({d, sub2}); }
        }
        while (!pq.empty()) {
            auto* sub2 = pq.top().second; pq.pop();
            if (DoesEdgeExist(sub1, sub2)) continue;
            auto* edge = new Edge(sub1, sub2);
            edge->current_load = 0.f;
            edge->max_load     = std::min(sub1->max_limit, sub2->max_limit);
            adj_power_substation[sub1].push_back({sub2, edge});
            adj_reverse_power[sub2].push_back({sub1, edge});
        }
    }
}
