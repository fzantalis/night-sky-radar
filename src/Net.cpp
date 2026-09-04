#include "Net.h"
#include "secrets.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

namespace {

bool     timeEverValid = false;
uint32_t lastRetryMs   = 0;

// Any timestamp before 2023-01-01 means the RTC has not been set.
constexpr int64_t SANE_EPOCH = 1672531200LL;

void startNtp() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

}  // namespace

namespace net {

void begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    startNtp();
}

void loop() {
    if (timeEverValid) return;

    // Re-check at most every 2 seconds until the first successful sync.
    const uint32_t now = millis();
    if (now - lastRetryMs < 2000) return;
    lastRetryMs = now;

    if (WiFi.status() != WL_CONNECTED) return;

    if (static_cast<int64_t>(::time(nullptr)) > SANE_EPOCH) {
        timeEverValid = true;
        Serial.println("[net] NTP sync acquired");
    }
}

bool wifiUp() {
    return WiFi.status() == WL_CONNECTED;
}

bool timeValid() {
    return timeEverValid;
}

int64_t nowUnix() {
    return static_cast<int64_t>(::time(nullptr));
}

}  // namespace net
