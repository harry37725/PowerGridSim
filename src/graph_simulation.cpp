// graph_simulation.cpp
// Responsibility: time-of-day + seasonal demand simulation.
#include "graph.h"

#include <algorithm>
#include <cmath>
#include <iostream>

static constexpr float kPi = 3.14159265f;

// ---------------------------------------------------------------------------
// seasonal_factor – cosine wave over the year, range [0.7, 1.3]
// Day 0 ≈ 1 Jan (winter peak ≈ 1.3), day 182 ≈ 1 Jul (summer trough ≈ 0.7)
// ---------------------------------------------------------------------------
float graph::seasonal_factor(int day) const {
    float frac = static_cast<float>(day) / 365.f;
    return 1.0f + 0.3f * cosf(2.f * kPi * frac);
}

std::string graph::get_month_name(int day) const {
    static const char* names[12] = {
        "January","February","March","April","May","June",
        "July","August","September","October","November","December"
    };
    return names[(day / 30) % 12];
}

int graph::get_month_number(int day) const {
    return ((day / 30) % 12) + 1;
}

// ---------------------------------------------------------------------------
// diurnal_factor – normalised [-1, +1] load modifier per consumer type / hour
// ---------------------------------------------------------------------------
float graph::diurnal_factor(const std::string& type, int hour) const {
    float h = static_cast<float>(hour);

    if (type == "residential") {
        float morning = 0.4f * sinf(2.f * kPi * (h -  2.f) / 24.f);
        float evening =        sinf(2.f * kPi * (h - 14.f) / 24.f);
        return (evening + morning) / 1.4f;
    }
    if (type == "hospital") {
        float phase = 12.f - 6.f;
        return 0.2f * sinf(2.f * kPi * (h - phase) / 24.f);
    }
    if (type == "institute") {
        if (hour < 6 || hour > 19) return -0.8f;
        float phase = 11.f - 6.f;
        return sinf(2.f * kPi * (h - phase) / 24.f);
    }
    // industrial (default)
    float phase = 14.f - 6.f;
    return sinf(2.f * kPi * (h - phase) / 24.f);
}

// ---------------------------------------------------------------------------
// target_demand_at_hour – combined diurnal × seasonal demand (kW), floor 5 kW
// ---------------------------------------------------------------------------
float graph::target_demand_at_hour(const Vertex_B* c, int hour, int day) const {
    float base, amp;
    if      (c->node_type == "residential") { base = 25.f; amp = 35.f; }
    else if (c->node_type == "hospital")    { base = 90.f; amp =  5.f; }
    else if (c->node_type == "institute")   { base = 15.f; amp = 55.f; }
    else /* industrial */                   { base = 80.f; amp = 10.f; }

    return std::max(5.f, (base + amp * diurnal_factor(c->node_type, hour)) * seasonal_factor(day));
}

// ---------------------------------------------------------------------------
// propagate_demand_change – distribute delta upward, proportional to edge capacity
// ---------------------------------------------------------------------------
void graph::propagate_demand_change(Vertex_A* node, float delta,
                                    std::unordered_set<Vertex_A*>& visited) {
    if (visited.count(node)) return;
    visited.insert(node);

    node->current_downstream_demand += delta;

    auto it = adj_reverse_power.find(node);
    if (it == adj_reverse_power.end()) { visited.erase(node); return; }  // power plant

    float total_cap = 0.f;
    for (const auto& [_, e] : it->second) total_cap += e->max_load;

    for (auto& [parent, edge] : it->second) {
        float prop = (total_cap > 0.01f)
                     ? edge->max_load / total_cap
                     : 1.f / static_cast<float>(it->second.size());
        float part = delta * prop;
        edge->current_load += part;
        propagate_demand_change(parent, part, visited);
    }
    visited.erase(node);
}

// ---------------------------------------------------------------------------
// advance_hour_demand – tick one hour, update all consumer demands.
// Returns the consumer with the largest absolute change (used for camera focus).
// ---------------------------------------------------------------------------
graph::Vertex_B* graph::advance_hour_demand() {
    int next_hour   = (sim_hour + 1) % 24;
    int current_day = sim_day;
    if (next_hour == 0) current_day = (sim_day + 1) % 365;

    Vertex_B* biggest   = nullptr;
    float     big_delta = 0.f;

    std::cout << "--- Time Step: " << get_month_name(current_day)
              << " (Day " << (current_day % 30 + 1) << "), hour "
              << sim_hour << " -> " << next_hour << " ---\n";

    for (auto& [substation, consumer_list] : adj_substion_consumer) {
        float sub_delta = 0.f;
        for (auto& [consumer, edge] : consumer_list) {
            float old_d = consumer->demand;
            float new_d = target_demand_at_hour(consumer, next_hour, current_day);
            float delta = new_d - old_d;
            if (fabsf(delta) < 0.01f) continue;

            float new_edge = edge->current_load + delta;
            if (new_edge < 0.f) { delta = -edge->current_load; new_edge = 0.f; }

            consumer->demand   = std::max(5.f, old_d + delta);
            edge->current_load = new_edge;
            sub_delta         += delta;

            if (fabsf(delta) > big_delta) { big_delta = fabsf(delta); biggest = consumer; }
        }
        if (fabsf(sub_delta) > 0.01f) {
            std::unordered_set<Vertex_A*> visited;
            propagate_demand_change(substation, sub_delta, visited);
        }
    }

    sim_hour = next_hour;
    if (next_hour == 0) sim_day = current_day;
    last_demand_increase_consumer = biggest;

    std::cout << "  Updated to " << get_month_name(sim_day)
              << " (Day " << (sim_day % 30 + 1) << "), hour " << sim_hour
              << ". Largest-delta consumer: "
              << (biggest ? biggest->node_type : "none") << '\n';
    return biggest;
}
