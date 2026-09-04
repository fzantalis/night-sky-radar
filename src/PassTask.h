#pragma once

#include <vector>

#include "Observer.h"
#include "Snapshot.h"
#include "Tle.h"

namespace passtask {

// Starts the background prediction task at low priority. Deliberately pinned
// to core 0, NOT core 1: Arduino's setup()/loop() (this board builds with
// board_build.arduino.memory_type = qio_opi, which selects
// CONFIG_ARDUINO_RUNNING_CORE=1 for ESP32-S3) already runs on core 1, and
// httpapi::loop()'s WebServer::handleClient() - which serves /api/scope to
// the render loop - is called synchronously from that same loop(). Pinning
// this task to core 1 as well would put 24h/175-object prediction sweeps on
// the exact core the render path depends on, defeating the entire point of a
// "second core" for this work. Core 0 keeps it genuinely off that core; only
// WiFi/BT (higher priority, mostly idle) share it.
void begin();

// Hands the task a fresh copy of the element sets to work from. Copies are
// taken under the mutex; the task then builds its OWN propagators, because
// Propagator::positionAt mutates internal state and cannot be shared with the
// render path.
void submit(const std::vector<Tle>& tles, const Observer& obs);

// Cached results, soonest first, with past passes dropped.
std::vector<Event> upcoming(int64_t nowUnix);

}  // namespace passtask
