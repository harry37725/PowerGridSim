// graph_core.cpp
// Responsibility: Vertex / Edge construction and network topology generation.
#include "graph.h"
#include "globals.h"

#include <algorithm>
#include <cstdlib>
#include <numeric>
#include <iostream>

// ---------------------------------------------------------------------------
// Vertex_B constructor
// BUG FIX: demand was 'int' in the original; the simulation applies fractional
// kW deltas every hour, so it is now 'float' to avoid silent truncation.
// ---------------------------------------------------------------------------
graph::Vertex_B::Vertex_B(std::string t) : node_type(std::move(t)) {
    if      (node_type == "residential") demand = static_cast<float>(resident_demand);
    else if (node_type == "industrial")  demand = static_cast<float>(industry_demand);
    else if (node_type == "institute")   demand = static_cast<float>(institute_demand);
    else                                 demand = static_cast<float>(hospital_demand);
}

// ---------------------------------------------------------------------------
// Edge constructors
// BUG FIX: the Vertex_A→Vertex_B constructor left 'loss' uninitialised in the
// original; it is now explicitly assigned.
// ---------------------------------------------------------------------------
graph::Edge::Edge(Vertex_A* from, Vertex_A* to)
    : loss(0.f), max_load(0.f), current_load(0.f)
{
    if (from->node_type == "power_plant" && to->node_type == "substation") {
        // Transmission loss: 2–5 kW
        loss = 2.f + static_cast<float>(rand()) /
               (static_cast<float>(RAND_MAX) / 3.f);
    } else if (from->node_type == "substation" && to->node_type == "substation") {
        // Inter-substation loss: 1–3 kW
        loss = 1.f + static_cast<float>(rand()) /
               (static_cast<float>(RAND_MAX) / 2.f);
    }
    // Backup sub↔sub links (added by add_realistic_connections) keep loss = 0.
}

graph::Edge::Edge(Vertex_A* /*from*/, Vertex_B* to) {
    // Distribution loss: 0.5–1.0 kW
    loss         = 0.5f + static_cast<float>(rand()) /
                   (static_cast<float>(RAND_MAX) / 0.5f);
    max_load     = 1.5f * to->demand;
    current_load = to->demand;
}

// ---------------------------------------------------------------------------
// distributor – weighted random split of total_count across divide_count bins.
// Each bin gets at least 1 to avoid zero-consumer substations.
// ---------------------------------------------------------------------------
std::vector<int> graph::distributor(int total_count, int divide_count) {
    if (divide_count <= 0) return {};

    int base     = std::max(1, total_count / divide_count);
    int min_w    = std::max(1, base - (3 * base) / 10);
    int max_w    = std::max(1, base + (3 * base) / 10);
    int range    = max_w - min_w + 1;

    std::vector<int> weights(divide_count);
    int weight_sum = 0;
    for (int& w : weights) { w = rand() % range + min_w; weight_sum += w; }
    if (weight_sum == 0) { std::fill(weights.begin(), weights.end(), 1); weight_sum = divide_count; }

    std::vector<int> dist(divide_count);
    int placed = 0;
    for (int i = 0; i < divide_count - 1; ++i) {
        dist[i] = static_cast<int>(
            static_cast<long long>(weights[i]) * total_count / weight_sum);
        if (dist[i] < 1) dist[i] = 1;   // guarantee at least 1 consumer
        placed += dist[i];
    }
    // Last bin absorbs the remainder (may be 0 for very tight distributions).
    dist[divide_count - 1] = std::max(0, total_count - placed);
    return dist;
}

// ---------------------------------------------------------------------------
// substation_consumer_connector
// ---------------------------------------------------------------------------
void graph::substation_consumer_connector(Vertex_A* sub, int value) {
    if (value <= 0) { adj_substion_consumer[sub] = {}; return; }

    int hospital_cnt   = static_cast<int>(0.2f * value);
    int industrial_cnt = static_cast<int>(0.1f * value);
    int institute_cnt  = static_cast<int>(0.2f * value);
    int resident_cnt   = value - (hospital_cnt + industrial_cnt + institute_cnt);

    float total_demand = 0.f;
    std::vector<std::pair<Vertex_B*, Edge*>> connections;
    connections.reserve(static_cast<std::size_t>(value));

    auto add = [&](const std::string& type, int count) {
        for (int i = 0; i < count; ++i) {
            auto* node = new Vertex_B(type);
            total_demand += node->demand;
            connections.push_back({node, new Edge(sub, node)});
        }
    };
    add("hospital",    hospital_cnt);
    add("industrial",  industrial_cnt);
    add("institute",   institute_cnt);
    add("residential", resident_cnt);

    sub->max_limit                 = total_demand * 3.0f;
    sub->current_downstream_demand = total_demand;
    adj_substion_consumer[sub]     = std::move(connections);
}

// ---------------------------------------------------------------------------
// map_powerplants_to_substations
// 40 % of substations connect directly to a power plant (primary).
// 60 % connect to any already-placed substation (secondary), forming a tree.
// ---------------------------------------------------------------------------
void graph::map_powerplants_to_substations(std::vector<Vertex_A*>& plants,
                                            std::vector<Vertex_A*>& subs) {
    int n_plants = static_cast<int>(plants.size());
    int n_subs   = static_cast<int>(subs.size());
    if (n_plants == 0 || n_subs == 0) return;

    int primary_cnt = static_cast<int>(0.4 * n_subs);
    if (primary_cnt == 0) primary_cnt = 1;

    std::vector<int> idx(n_subs);
    std::iota(idx.begin(), idx.end(), 0);
    std::shuffle(idx.begin(), idx.end(), g_rng);

    int placed = 0;

    // 1. Plant → primary substations
    for (int i = 0; i < primary_cnt && placed < n_subs; ++i, ++placed) {
        Vertex_A* plant = plants[i % n_plants];
        Vertex_A* sub   = subs[idx[placed]];
        auto*     edge  = new Edge(plant, sub);
        edge->current_load = sub->current_downstream_demand;
        edge->max_load     = 1.5f * sub->max_limit;
        adj_power_substation[plant].push_back({sub, edge});
        adj_reverse_power[sub].push_back({plant, edge});
    }

    // 2. Secondary substations → any already-placed substation
    while (placed < n_subs) {
        if (placed == 0) break;   // safety (shouldn't happen given primary_cnt ≥ 1)
        Vertex_A* parent = subs[idx[rand() % placed]];
        Vertex_A* child  = subs[idx[placed]];
        auto*     edge   = new Edge(parent, child);
        parent->max_limit                 += child->max_limit;
        parent->current_downstream_demand += child->current_downstream_demand;
        edge->current_load = child->current_downstream_demand;
        edge->max_load     = 2.0f * child->max_limit;
        adj_power_substation[parent].push_back({child, edge});
        adj_reverse_power[child].push_back({parent, edge});
        ++placed;
    }

    // 3. Set power-plant limits from their direct children
    for (auto& [plant, children] : adj_power_substation) {
        if (plant->node_type != "power_plant") continue;
        float sum_lim = 0.f, sum_dem = 0.f;
        for (auto& [sub, _] : children) { sum_lim += sub->max_limit; sum_dem += sub->current_downstream_demand; }
        plant->max_limit                 = sum_lim * 1.5f;
        plant->current_downstream_demand = sum_dem;
    }
}

// ---------------------------------------------------------------------------
// mapping – public entry point: build the complete topology.
// ---------------------------------------------------------------------------
void graph::mapping(int pp_count, int sub_count, int consumer_count) {
    std::vector<Vertex_A*> plants;
    plants.reserve(pp_count);
    for (int i = 0; i < pp_count; ++i)
        plants.push_back(new Vertex_A("power_plant"));

    std::vector<Vertex_A*> subs;
    subs.reserve(sub_count);
    for (int n : distributor(consumer_count, sub_count)) {
        auto* s = new Vertex_A("substation");
        substation_consumer_connector(s, n);
        subs.push_back(s);
    }
    map_powerplants_to_substations(plants, subs);
}
