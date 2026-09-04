#include <Arduino.h>
#include <esp_heap_caps.h>

#include "CoreAlloc.h"
#include "Config.h"
#include "HttpApi.h"
#include "Net.h"
#include "PassTask.h"
#include "ScopeService.h"

static Snapshot provide() { return scope::build(); }

namespace {

// Backs corealloc::alloc/free (lib/core/src/CoreAlloc.h) with PSRAM.
// lib/core/ cannot include esp_heap_caps.h itself, so Propagator's SGP4
// payloads go through this indirection instead - see CoreAlloc.h and the
// Task 6 note in Propagator.h/.cpp.
void* psramAlloc(std::size_t n) {
    return heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
}

void psramFree(void* p) {
    heap_caps_free(p);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n[boot] sky radar");

    // Must run before anything that can call Propagator::init() - scope::begin()
    // below loads cached element sets and rebuilds `tracked` immediately.
    corealloc::setAllocator(&psramAlloc, &psramFree);

    config::begin();

    net::begin();

    // httpapi::begin() mounts LittleFS; scope::begin() needs that filesystem
    // mounted before it tries to load the cached element sets, so this must
    // run first.
    httpapi::begin(&provide);

    // Must run after corealloc::setAllocator() above (the prediction task
    // builds its own Propagators, which allocate their SGP4 payloads through
    // corealloc::alloc() - see the non-atomic g_alloc/g_free note in
    // CoreAlloc.cpp) and, just as importantly, before scope::begin() below:
    // scope::begin() loads the cached element sets and immediately calls
    // passtask::submit() with them, and submit() is a silent no-op until
    // begin() has created the task's mutex. Submitting after begin() means
    // the very first boot has to wait out a full refresh cycle (up to
    // REFRESH_HOURS) before any pass is predicted.
    Serial.printf("[boot] heap %u, psram %u before passtask::begin()\n",
                  ESP.getFreeHeap(), ESP.getFreePsram());
    passtask::begin();
    Serial.printf("[boot] heap %u, psram %u after passtask::begin()\n",
                  ESP.getFreeHeap(), ESP.getFreePsram());

    scope::begin();

    Serial.printf("[boot] psram: %u bytes\n",
                  static_cast<unsigned>(ESP.getPsramSize()));
}

void loop() {
    net::loop();
    scope::loop();
    httpapi::loop();

    // All three return promptly, so without this loop() spins at 100% on
    // core 1 and permanently starves the idle task (watchdog feed, RTOS
    // bookkeeping) and, at M2, any low-priority task pinned to this core.
    delay(1);
}
