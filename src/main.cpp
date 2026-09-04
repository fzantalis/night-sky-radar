#include <Arduino.h>

#include "Config.h"
#include "HttpApi.h"
#include "Net.h"
#include "ScopeService.h"

static Snapshot provide() { return scope::build(); }

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n[boot] sky radar");

    config::begin();

    net::begin();

    // httpapi::begin() mounts LittleFS; scope::begin() needs that filesystem
    // mounted before it tries to load the cached element sets, so this must
    // run first.
    httpapi::begin(&provide);
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
