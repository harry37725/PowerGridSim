#include "globals.h"
#include <random>

// Demand base-values – overwritten by the UI before graph construction.
int resident_demand  = 25;
int hospital_demand  = 90;
int industry_demand  = 80;
int institute_demand = 15;

// Single shared RNG, seeded from hardware entropy at program start.
static std::random_device s_rd;
std::mt19937 g_rng(s_rd());
