#pragma once
#include "Snapshot.h"

namespace httpapi {

using SnapshotProvider = Snapshot (*)();

void begin(SnapshotProvider provider);
void loop();

}  // namespace httpapi
