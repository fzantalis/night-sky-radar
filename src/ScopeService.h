#pragma once
#include "Snapshot.h"

namespace scope {

void begin();

// Drives TLE refresh scheduling and trail sampling. Non-blocking except during
// an actual fetch.
void loop();

Snapshot build();

// Discards all position history. Call when the observer location changes - existing
// trail points were computed for the old site and are meaningless for the new one.
void clearTrails();

}  // namespace scope
