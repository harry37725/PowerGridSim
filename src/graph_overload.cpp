// graph_overload.cpp
// Responsibility: overload detection, widest-path re-routing, throttling,
//                and manual capacity upgrades.
#include "graph.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>

// ---------------------------------------------------------------------------
// propagate_limit_increase – push a capacity increase upward through parents
// ---------------------------------------------------------------------------
void graph::propagate_limit_increase(Vertex_A* node, float increase,
                                     std::unordered_set<Vertex_A*>& visited) {
    if (visited.count(node)) return;
    visited.insert(node);
    auto it = adj_reverse_power.find(node);
    if (it == adj_reverse_power.end()) { visited.erase(node); return; }

    for (auto& [parent, edge] : it->second) {
        float inc = (parent->node_type == "power_plant") ? increase * 1.2f : increase;
        parent->max_limit += inc;
        edge->max_load    += increase;
        propagate_limit_increase(parent, inc, visited);
    }
    visited.erase(node);
}

// ---------------------------------------------------------------------------
// try_add_capacity_path – helper for the widest-path search
// ---------------------------------------------------------------------------
void graph::try_add_capacity_path(
    const PathState& cur, Vertex_A* next, Edge* edge,
    std::priority_queue<PathState>& pq,
    std::unordered_map<Vertex_A*, float>& cap_map)
{
    for (const auto& [n, _] : cur.path)    // cycle check
        if (n == next) return;

    float avail   = edge->max_load - edge->current_load;
    float new_cap = std::min(cur.min_available_capacity, avail);
    if (new_cap <= 0.01f) return;

    if (!cap_map.count(next) || new_cap > cap_map[next]) {
        cap_map[next] = new_cap;
        auto p = cur.path;
        p.push_back({next, edge});
        pq.push(PathState(new_cap, std::move(p)));
    }
}

// ---------------------------------------------------------------------------
// find_highest_capacity_path – Dijkstra variant that maximises bottleneck capacity
// ---------------------------------------------------------------------------
std::pair<float, std::vector<std::pair<graph::Vertex_A*, graph::Edge*>>>
graph::find_highest_capacity_path(Vertex_A* source, Vertex_A* target) {
    std::priority_queue<PathState> pq;
    std::unordered_map<Vertex_A*, float> cap_map;

    std::vector<std::pair<Vertex_A*, Edge*>> init = {{source, nullptr}};
    pq.push(PathState(std::numeric_limits<float>::max(), init));
    cap_map[source] = std::numeric_limits<float>::max();

    float best_cap = 0.f;
    std::vector<std::pair<Vertex_A*, Edge*>> best_path;

    while (!pq.empty()) {
        PathState cur = pq.top(); pq.pop();
        Vertex_A* node    = cur.path.back().first;
        float     cur_cap = cur.min_available_capacity;

        if (cap_map.count(node) && cur_cap < cap_map[node]) continue;

        if (node == target) {
            if (cur_cap > best_cap) { best_cap = cur_cap; best_path = cur.path; }
            continue;
        }

        auto it_fwd = adj_power_substation.find(node);
        if (it_fwd != adj_power_substation.end())
            for (auto& [nb, e] : it_fwd->second)
                try_add_capacity_path(cur, nb, e, pq, cap_map);

        auto it_rev = adj_reverse_power.find(node);
        if (it_rev != adj_reverse_power.end())
            for (auto& [nb, e] : it_rev->second)
                try_add_capacity_path(cur, nb, e, pq, cap_map);
    }
    return {best_cap, best_path};
}

// ---------------------------------------------------------------------------
// detect_overloads – Stage 1: scan and record overloaded edges / nodes.
// ---------------------------------------------------------------------------
bool graph::detect_overloads() {
    overloaded_edges.clear();
    pending_problems.clear();
    edge_being_fixed = nullptr;
    fix_path.clear();
    throttled_nodes_visual.clear();

    constexpr float THRESHOLD = 0.75f;

    // 1a. Edge-ratio overloads
    for (auto& [source, neighbors] : adj_power_substation)
        for (auto& [target, edge] : neighbors)
            if (edge->max_load > 0.01f) {
                float ratio = edge->current_load / edge->max_load;
                if (ratio >= THRESHOLD) {
                    pending_problems.push_back({ratio, source, target, edge});
                    overloaded_edges.insert(edge);
                }
            }

    // 1b. Node-capacity overloads
    for (auto& [node, parents] : adj_reverse_power) {
        if (node->node_type == "power_plant" || node->max_limit < 0.01f) continue;
        float ratio = node->current_downstream_demand / node->max_limit;
        if (ratio <= 1.0f) continue;

        Vertex_A* best_src  = nullptr;
        Edge*     best_edge = nullptr;
        float     best_load = -1.f;
        for (auto& [parent, edge] : parents)
            if (edge->current_load > best_load)
            { best_load = edge->current_load; best_src = parent; best_edge = edge; }

        if (best_edge && best_src && !overloaded_edges.count(best_edge)) {
            pending_problems.push_back({ratio + 1.0f, best_src, node, best_edge});
            overloaded_edges.insert(best_edge);
        }
    }

    if (pending_problems.empty()) return false;
    std::sort(pending_problems.begin(), pending_problems.end(), std::greater<OverloadInfo>());
    return true;
}

// ---------------------------------------------------------------------------
// fix_overloading_problem (internal)
// ---------------------------------------------------------------------------
bool graph::fix_overloading_problem(Vertex_A* source, Vertex_A* target,
                                    Edge* oe, bool visualize_this_fix) {
    if (!oe) return false;

    constexpr float EPS       = 0.01f;
    constexpr float OL_RATIO  = 0.75f;
    constexpr float TGT_FACTOR= 0.70f;

    float node_excess = (target->max_limit > EPS)
                        ? target->current_downstream_demand - target->max_limit : 0.f;
    float cur_factor  = (oe->max_load > EPS) ? oe->current_load / oe->max_load : 1.f;
    float edge_excess = oe->current_load - TGT_FACTOR * oe->max_load;
    float to_reduce   = std::max(node_excess, edge_excess);

    if ((node_excess <= EPS) && (cur_factor < OL_RATIO)) { overloaded_edges.erase(oe); return false; }
    if (to_reduce < EPS) return false;

    auto [max_cap, best_path] = find_highest_capacity_path(source, target);

    bool  usable   = false;
    float rerouted = 0.f;

    if (best_path.size() > 1) {
        rerouted = std::min(to_reduce, max_cap);
        if (rerouted < to_reduce * 0.20f) {
            std::cout << "Load Balancing: path capacity " << max_cap
                      << " < 20% of required " << to_reduce << ". Throttling.\n";
        } else {
            bool stable = true;
            for (std::size_t i = 1; i < best_path.size(); ++i) {
                Edge* pe = best_path[i].second;
                if (pe->max_load > EPS &&
                    (pe->current_load + rerouted) / pe->max_load >= OL_RATIO)
                { stable = false; std::cout << "Load Balancing: path unstable. Throttling.\n"; break; }
            }
            if (stable) usable = true;
        }
    }

    if (usable) {
        std::cout << std::fixed << std::setprecision(2)
                  << "--- Load Balancing ---\n"
                  << "  Edge load: " << oe->current_load << "/" << oe->max_load
                  << " (" << cur_factor * 100.f << "%). Rerouting " << rerouted << " kW.\n";

        oe->current_load -= rerouted;
        for (std::size_t i = 1; i < best_path.size(); ++i)
            best_path[i].second->current_load += rerouted;

        if (oe->current_load / oe->max_load < OL_RATIO) {
            overloaded_edges.erase(oe);
            std::cout << "  Status: FIXED\n";
        } else {
            std::cout << "  Status: PARTIAL FIX (still overloaded)\n";
        }

        if (visualize_this_fix) { edge_being_fixed = oe; fix_path = best_path; }
        return true;
    }

    // Throttle fallback
    std::cout << std::fixed << std::setprecision(2)
              << "  -> Scheduling demand reduction at node " << target
              << " by " << to_reduce << " kW\n";
    overloaded_edges.erase(oe);
    nodes_to_throttle[target] += to_reduce;
    return false;
}

// ---------------------------------------------------------------------------
// apply_overload_fixes – Stage 2/3
// ---------------------------------------------------------------------------
void graph::apply_overload_fixes() {
    if (pending_problems.empty()) return;
    bool fix_applied = false;
    for (auto& prob : pending_problems) {
        if (!overloaded_edges.count(prob.edge)) continue;
        if (fix_overloading_problem(prob.source, prob.target, prob.edge, !fix_applied))
            fix_applied = true;
        else if (!nodes_to_throttle.empty()) break;
    }
    pending_problems.clear();
}

void graph::overloading_edge() { detect_overloads(); apply_overload_fixes(); }

// ---------------------------------------------------------------------------
// apply_demand_reduction_updates – call every frame; drains nodes_to_throttle
// ---------------------------------------------------------------------------
void graph::apply_demand_reduction_updates() {
    if (nodes_to_throttle.empty()) return;
    std::cout << "--- Throttling ---\n";

    for (auto& [sub, required] : nodes_to_throttle) {
        auto it = adj_substion_consumer.find(sub);
        if (it == adj_substion_consumer.end() || required <= 0.01f) continue;

        float total = 0.f;
        for (auto& [c, _] : it->second) total += c->demand;
        if (total < 0.01f) { std::cout << "  Node has 0 demand; skipping.\n"; continue; }

        float applied = 0.f;
        for (auto& [consumer, edge] : it->second) {
            float delta = -required * (consumer->demand / total);
            if (consumer->demand + delta < 0.f) delta = -consumer->demand;
            if (delta < -0.01f) {
                consumer->demand   += delta;
                edge->current_load += delta;
                applied            += delta;
            }
        }
        if (applied < -0.01f) {
            throttled_nodes_visual.insert(sub);
            std::cout << std::fixed << std::setprecision(2)
                      << "  Throttled node " << sub << " by " << -applied << " kW\n";
            std::unordered_set<Vertex_A*> visited;
            propagate_demand_change(sub, applied, visited);
        }
    }
    nodes_to_throttle.clear();
    std::cout << "------------------\n";
}

// ---------------------------------------------------------------------------
// check_node_overloads – refresh node_overloads_visual
// ---------------------------------------------------------------------------
void graph::check_node_overloads() {
    node_overloads_visual.clear();
    std::unordered_set<Vertex_A*> all;
    for (auto& [p, ch] : adj_power_substation) {
        all.insert(p);
        for (auto& [c, _] : ch) all.insert(c);
    }
    for (auto& [p, _] : adj_substion_consumer) all.insert(p);

    for (auto* n : all)
        if (n->node_type != "power_plant" &&
            n->current_downstream_demand > n->max_limit)
            node_overloads_visual.insert(n);
}

// ---------------------------------------------------------------------------
// upgrade_selected_node_limit – last-resort manual capacity upgrade
// ---------------------------------------------------------------------------
void graph::upgrade_selected_node_limit(Vertex_A* node) {
    if (!node || node->node_type == "power_plant") {
        std::cout << "Upgrade: no valid substation selected.\n"; return;
    }
    if (!node_overloads_visual.count(node)) {
        std::cout << "Upgrade: node is not overloaded.\n"; return;
    }
    float deficit  = node->current_downstream_demand - node->max_limit;
    float increase = deficit * 1.05f;   // exact deficit + 5% buffer

    std::cout << std::fixed << std::setprecision(2)
              << "--- Node Upgrade ---\n  Old limit: " << node->max_limit;
    node->max_limit += increase;
    std::cout << " kW  New limit: " << node->max_limit << " kW\n";

    std::unordered_set<Vertex_A*> visited;
    propagate_limit_increase(node, increase, visited);
    check_node_overloads();
}
