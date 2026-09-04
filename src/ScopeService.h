#pragma once
#include "Snapshot.h"

namespace scope {

void begin();

// Drives TLE refresh scheduling and trail sampling. Non-blocking except during
// an actual fetch.
void loop();

Snapshot build();

// The last snapshot cached by loop() (rebuilt at most once per second).
// Prefer this over build() for anything that doesn't need a guaranteed-fresh
// propagation pass - an HTTP request or the status LED calling build()
// directly would each trigger their own pass over every tracked object.
const Snapshot& currentSnapshot();

// Discards all position history. Call when the observer location changes - existing
// trail points were computed for the old site and are meaningless for the new one.
void clearTrails();

}  // namespace scope
