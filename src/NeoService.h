#pragma once

#include <cstdint>
#include <vector>

#include "Neo.h"

// M5 - keeps the near-Earth close-approach list fresh and cached.
//
// Deliberately much simpler than ScopeService: the whole 30-day list is about
// 1.2 KB, so there is no streaming, no per-group validation and no element-set
// bookkeeping. What it does share is the parse-before-commit discipline - a
// response is only allowed to replace the cache after it has parsed as a
// genuine CAD payload.
namespace neoservice {

// The rim of the NEO dial, in lunar distances, and the span of the window.
// Both travel into the snapshot so the renderer can label the dial.
constexpr double  NEO_RIM_LD      = 10.0;
constexpr int     NEO_WINDOW_DAYS = 30;

// Close-approach predictions move far more slowly than orbital elements do -
// nothing inside a 30-day window changes materially over half a day - so this
// matches the TLE cadence rather than needing its own.
constexpr double  NEO_REFRESH_HOURS = 12.0;

// Same cap as MAX_BLIPS: the dial cannot usefully show more.
constexpr int     MAX_NEOS = 12;

// Loads the cached list. Requires LittleFS to be mounted already - call after
// httpapi::begin(), for the same reason scope::begin() does.
void begin();

// Fetches when due. Blocking, and only ever called from loop() on core 0.
void loop(int64_t nowUnix);

const std::vector<neo::Approach>& approaches();

// Hours since the cached list was fetched; negative when unknown.
double ageHours(int64_t nowUnix);

}  // namespace neoservice
