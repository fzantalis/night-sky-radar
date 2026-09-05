#pragma once
#include "Snapshot.h"

namespace scope {

void begin();

// Drives TLE refresh scheduling and trail sampling. Non-blocking except during
// an actual fetch.
void loop();

Snapshot build();

// M5 - which projection build() produces. SKY plots where to look right now;
// NEO plots asteroid close approaches by distance and date. Both are the same
// dial fed a different projection (design spec section 3.1), so switching is
// just this flag - there is no second renderer and no second physics path.
//
// NEO mode deliberately does not require an observer location: a close
// approach is an Earth-centred event, so the dial is meaningful on a device
// that has never been told where it is.
ScopeMode mode();
void setMode(ScopeMode m);
void toggleMode();

// The last snapshot cached by loop() (rebuilt at most once per second).
// Prefer this over build() for anything that doesn't need a guaranteed-fresh
// propagation pass - an HTTP request or the status LED calling build()
// directly would each trigger their own pass over every tracked object.
const Snapshot& currentSnapshot();

// Discards all position history. Call when the observer location changes - existing
// trail points were computed for the old site and are meaningless for the new one.
void clearTrails();

}  // namespace scope
