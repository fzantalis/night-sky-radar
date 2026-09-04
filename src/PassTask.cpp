#include "PassTask.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <algorithm>

#include "Net.h"
#include "PassPredictor.h"
#include "Propagator.h"
#include "StdMagTable.h"

namespace {

constexpr uint32_t RECOMPUTE_INTERVAL_MS = 45UL * 60UL * 1000UL;
constexpr int      HORIZON_HOURS         = 24;
constexpr int      MAX_EVENTS            = 8;

// How long taskLoop() waits before retrying when a cycle found no input yet.
// This task is created (begin()) strictly before scope::begin() runs (see the
// ordering note in main.cpp), and scope::begin() is what makes the first
// submit() call - it has real work to do first (mounting LittleFS, parsing
// up to ~200 TLEs, building that many Propagators) before it gets there. This
// task, pinned to an otherwise-idle core, reliably starts running and takes
// its first mutex lock before any of that finishes, so the very first
// computeOnce() call almost always finds haveInput still false. Sleeping the
// full RECOMPUTE_INTERVAL_MS after that empty cycle - as if it were a normal
// "just finished a sweep" cycle - would leave the task silently dead for 45
// minutes on every single boot. Retrying quickly here instead means the task
// picks up the first real submit() within about a second of it happening,
// while a cycle that DID have input still backs off for the full interval.
constexpr uint32_t NO_INPUT_RETRY_MS = 1000UL;

SemaphoreHandle_t mutex = nullptr;

// Input side, written by submit().
std::vector<Tle> pendingTles;
Observer         pendingObs;
bool             haveInput = false;

// Output side, written by the task. `computedAt` is what keeps the countdowns
// honest: startsIn is stored relative to it and rebased on every read, so a
// result computed 40 minutes ago still ticks down correctly.
std::vector<Event> results;
int64_t            computedAt = 0;

void yieldToScheduler() {
    vTaskDelay(1);
}

// Returns false without doing any propagation if submit() has not delivered
// anything yet - see the NO_INPUT_RETRY_MS note above for why taskLoop()
// needs to tell this case apart from "just finished a real sweep".
bool computeOnce() {
    // The project's standing rule: never propagate without a valid clock.
    // ScopeService enforces this in two places; PassTask did not, and the
    // omission was live. PassTask receives its TLE input from the LittleFS
    // cache during scope::begin(), which completes BEFORE NTP syncs. Without
    // this guard the task ran a full 24 h sweep against a 1970 clock: it found
    // 8 meaningless "passes", stamped computedAt with a near-zero value, and
    // every countdown then rebased to roughly minus 56 years and was dropped
    // by upcoming() as already-started - so /api/scope reported zero events
    // while the serial log cheerfully claimed 8. Returning false retries in
    // NO_INPUT_RETRY_MS rather than sleeping the full recompute interval.
    if (!net::timeValid()) return false;

    std::vector<Tle> tles;
    Observer obs;

    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return false;
    if (!haveInput) { xSemaphoreGive(mutex); return false; }
    tles = pendingTles;      // copy out, then release immediately
    obs  = pendingObs;
    xSemaphoreGive(mutex);

    const int64_t now = net::nowUnix();
    std::vector<Event> found;

    for (const Tle& t : tles) {
        // Cheap rejection before any propagation.
        if (!couldEverRise(t.inclinationDeg, t.meanMotionRevPerDay, obs.latDeg)) {
            continue;
        }

        // A propagator private to this task. Never share one with build().
        Propagator prop;
        if (!prop.init(t)) continue;

        const std::vector<Pass> passes =
            predictPasses(prop, obs, stdMagFor(t.satnum),
                          now, HORIZON_HOURS, &yieldToScheduler);

        for (const Pass& p : passes) {
            if (!p.visible) continue;          // only visible passes are events
            Event e;
            e.name     = t.name;
            e.startsIn = p.riseUnix - now;
            e.maxEl    = p.maxElDeg;
            e.visible  = true;
            found.push_back(e);
        }

        yieldToScheduler();
    }

    std::sort(found.begin(), found.end(),
              [](const Event& a, const Event& b) { return a.startsIn < b.startsIn; });
    if (static_cast<int>(found.size()) > MAX_EVENTS) {
        found.resize(static_cast<size_t>(MAX_EVENTS));
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        results    = found;
        computedAt = now;
        xSemaphoreGive(mutex);
    }

    Serial.printf("[pass] %u visible passes in the next %d h\n",
                  static_cast<unsigned>(found.size()), HORIZON_HOURS);
    return true;
}

void taskLoop(void*) {
    for (;;) {
        const bool didWork = computeOnce();
        vTaskDelay(pdMS_TO_TICKS(didWork ? RECOMPUTE_INTERVAL_MS : NO_INPUT_RETRY_MS));
    }
}

}  // namespace

namespace passtask {

void begin() {
    mutex = xSemaphoreCreateMutex();
    // 8 KB stack: SGP4 uses a large elsetrec on the stack in places. Core 0 -
    // see the note in PassTask.h for why this is deliberately NOT core 1 -
    // priority 1, below anything time-critical, so it can never starve
    // rendering or the WiFi/BT stack that also lives on this core.
    xTaskCreatePinnedToCore(taskLoop, "passes", 8192, nullptr, 1, nullptr, 0);
}

void submit(const std::vector<Tle>& tles, const Observer& obs) {
    if (mutex == nullptr) return;
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return;
    pendingTles = tles;
    pendingObs  = obs;
    haveInput   = true;
    xSemaphoreGive(mutex);
}

std::vector<Event> upcoming(int64_t nowUnix) {
    std::vector<Event> out;
    if (mutex == nullptr) return out;
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) return out;

    // Results were computed at `computedAt`; rebase every countdown onto the
    // caller's clock and drop passes that have already started. A countdown
    // that does not tick is worse than no countdown.
    const int64_t elapsed = nowUnix - computedAt;
    for (const Event& e : results) {
        Event r = e;
        r.startsIn = e.startsIn - elapsed;
        if (r.startsIn > 0) out.push_back(r);
    }

    xSemaphoreGive(mutex);
    return out;
}

}  // namespace passtask
