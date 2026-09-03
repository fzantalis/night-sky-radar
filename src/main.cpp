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

    // TEMPORARY placeholder for verification only - Task 11 replaces this with
    // the /config form. These are NOT the user's real coordinates.
    config::setObserver(Observer{37.9838, 23.7275, 0.100});

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
}
