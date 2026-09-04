#pragma once

#include "Tle.h"

// Discriminates a freshly-launched, still orbit-raising Starlink batch (a
// "train") from CelesTrak's ~8000-object `starlink` group, without ever
// trying to read a date out of the COSPAR designator - a designator encodes
// only the launch YEAR and the Nth launch of that year, never a date (see
// the M3 plan's spec correction: docs/superpowers/plans/2026-09-04-sky-radar-
// m3-starlink.md). Mean motion is the physically correct discriminator
// instead: a freshly deployed batch sits near 350 km and is still raising
// orbit, while the operational constellation has settled to ~550 km.
namespace trainfilter {

// Verified against a live CelesTrak `starlink` fetch on 2026-09-04
// (10,721 objects): the operational constellation's mean motion clusters
// tightly around a median of 15.317 rev/day (8441/10721 objects round to
// 15.3), with a clean break above that. 15.35 sits just above that break and
// below every still-climbing object seen in that fetch - see
// docs/superpowers/reports/2026-09-04-m3-report.md for the full distribution
// this was tuned against.
constexpr double kDefaultMinMeanMotionRevPerDay = 15.35;

struct Params {
    double minMeanMotionRevPerDay = kDefaultMinMeanMotionRevPerDay;
};

// `currentYear` is the caller's civil year (UTC): the window is
// [currentYear-1, currentYear], i.e. "launched this year or last, and still
// hot" - never derived from anything the designator itself encodes.
//
// Excludes Starlink "Direct to Cell" satellites by name (CelesTrak tags them
// "[DTC]" in the name field) regardless of mean motion. DTC satellites fly
// permanently lower than the rest of the constellation *by design*, not
// because they were recently launched: in the same 2026-09-04 fetch, ~280
// DTC objects spread across 15+ launches from January through the present
// all sat at 15.70-15.71 rev/day - well past this threshold - with no
// clustering between them. The one genuine train in that fetch (24 members
// sharing a single launch designator, mean motion within a 0.002 rev/day
// band of each other) sat at the very end of the group in catalog-number
// order. Without this exclusion, a caller's maxKeep cap fills with months-
// old DTC members long before the stream ever reaches an actual train.
bool isTrainCandidate(const Tle& t, int currentYear, const Params& params = Params());

}  // namespace trainfilter
