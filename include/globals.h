#pragma once
#include <random>
#include <cstddef>
#include <utility>
#include <functional>

// ---------------------------------------------------------------------------
// Global demand base-values (kW).
// Set from the UI before graph construction; read by Vertex_B constructor.
// ---------------------------------------------------------------------------
extern int resident_demand;
extern int hospital_demand;
extern int industry_demand;
extern int institute_demand;

// Shared Mersenne-Twister RNG (seeded once in main).
extern std::mt19937 g_rng;

// ---------------------------------------------------------------------------
// Hash helper for std::pair<int,int> – required by spatial-grid maps.
// ---------------------------------------------------------------------------
struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const noexcept {
        std::size_t h1 = std::hash<T1>{}(p.first);
        std::size_t h2 = std::hash<T2>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};
