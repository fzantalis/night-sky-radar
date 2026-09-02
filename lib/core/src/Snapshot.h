#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Hard cap from spec section 7.3. The core ranks and truncates; renderers never
// decide what matters.
constexpr int MAX_BLIPS = 12;

enum class ScopeStatus {
    Ok,
    NoTime,       // NTP has never succeeded; do not plot anything
    NoLocation,   // observer position unset
    Offline       // running from cached elements
};

struct Blip {
    int         id        = 0;
    std::string name;
    double      r         = 0.0;   // 0 = centre, 1 = rim
    double      theta     = 0.0;   // degrees, sky frame, 0 = true north
    double      magnitude = 99.0;
    bool        visible   = false; // reserved for M2; always false at M1
    std::vector<std::pair<double, double>> trail;  // (r, theta) history
};

struct Snapshot {
    int64_t     t            = 0;
    ScopeStatus status       = ScopeStatus::NoTime;
    double      tleAgeHours  = -1.0;   // negative means unknown
    std::vector<Blip> blips;
};

// Sorts visible objects first, then by ascending r (higher in the sky first),
// and truncates to MAX_BLIPS.
void rankAndCap(Snapshot& s);

std::string toJson(const Snapshot& s);
