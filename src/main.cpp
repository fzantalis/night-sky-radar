#include <Arduino.h>

#include "Config.h"
#include "Net.h"

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n[boot] sky radar");

    config::begin();
    net::begin();

    Serial.printf("[boot] psram: %u bytes\n",
                  static_cast<unsigned>(ESP.getPsramSize()));
    Serial.printf("[boot] location set: %s\n",
                  config::hasLocation() ? "yes" : "no");
}

void loop() {
    net::loop();

    static uint32_t last = 0;
    if (millis() - last >= 5000) {
        last = millis();
        Serial.printf("[status] wifi=%d time=%d unix=%lld\n",
                      net::wifiUp() ? 1 : 0,
                      net::timeValid() ? 1 : 0,
                      static_cast<long long>(net::nowUnix()));
    }
}
