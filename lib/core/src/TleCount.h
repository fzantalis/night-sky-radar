#pragma once

// Plausibility gate for a freshly-parsed TLE group body, pulled out of
// ScopeService so it is unit-testable without Arduino/ESP-IDF. A caller
// fetches raw text over TLS, parses it, and must decide whether the parsed
// element count is credible enough to overwrite the cached copy of *that
// same group* - never a different group, and never a combined total across
// groups (a small group checked against a large combined total rejects
// itself forever; see the M2 Task 6 fix-round-1 finding this exists for).
namespace tlecount {

// `count` is what parseTleText() just returned for the newly-fetched body.
// `previousCount` is the last count this same group validated successfully
// (0 means "no history yet" - e.g. first boot, or first fetch of a group
// that has never been cached).
//
// Rejects:
//   - count == 0 outright, unconditionally (an empty parse is never usable,
//     even as a first fetch).
//   - count less than half of previousCount, when previousCount > 0 (catches
//     a partial download that still happens to parse cleanly, e.g. a
//     truncated TLS read cut mid-file at a triple boundary).
// Accepts everything else, including any positive count when previousCount
// is 0 (nothing to compare against yet).
bool tleCountIsPlausible(int count, int previousCount);

}  // namespace tlecount
