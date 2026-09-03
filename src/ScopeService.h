#pragma once
#include "Snapshot.h"

namespace scope {

void begin();

// Drives TLE refresh scheduling and trail sampling. Non-blocking except during
// an actual fetch.
void loop();

Snapshot build();

}  // namespace scope
