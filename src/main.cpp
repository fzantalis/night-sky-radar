#include <Arduino.h>

#include "Config.h"
#include "Net.h"
#include "HttpApi.h"
#include "Snapshot.h"
#include "TleStore.h"

// Real status handling, no blips yet. Task 9 replaces this with ScopeService.
static Snapshot buildSnapshot() {
    Snapshot s;
    s.t = net::nowUnix();

    if (!net::timeValid()) {
        s.status = ScopeStatus::NoTime;
        return s;
    }
    if (!config::hasLocation()) {
        s.status = ScopeStatus::NoLocation;
        return s;
    }

    s.status = net::wifiUp() ? ScopeStatus::Ok : ScopeStatus::Offline;
    return s;
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n[boot] sky radar");

    config::begin();
    net::begin();
    httpapi::begin(&buildSnapshot);
    tlestore::begin();

    Serial.printf("[boot] psram: %u bytes\n",
                  static_cast<unsigned>(ESP.getPsramSize()));
}

void loop() {
    net::loop();
    httpapi::loop();
}
