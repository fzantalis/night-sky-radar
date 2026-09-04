#pragma once
#include "Snapshot.h"

namespace statusled {

void begin();

// Call from loop(). Rate-limits internally; safe to call every iteration.
void update(const Snapshot& s);

}  // namespace statusled
