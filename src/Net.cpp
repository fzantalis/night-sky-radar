#include "Net.h"
#include "secrets.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

namespace {

bool     timeEverValid = false;
bool     ipLogged      = false;
uint32_t lastRetryMs   = 0;

// Any timestamp before 2023-01-01 means the RTC has not been set.
constexpr int64_t SANE_EPOCH = 1672531200LL;

void startNtp() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

#if defined(STATIC_IP)
// Static addressing exists because a router can associate a client perfectly
// and still never hand it a lease. That was diagnosed here by watching the WiFi
// events: the station reached ASSOCIATED and then sat for 25 s with no GOT_IP,
// while the identical attempt with a fixed address came up instantly. The radio
// was never the problem, and neither was the password - only DHCP.
//
// It also ends a smaller nuisance. A lease that moves (this device was handed
// .59, .60 and .31 within one afternoon) turns "open the web view" into a
// subnet scan every time.
bool     staticActive   = false;
bool     fellBackToDhcp = false;
uint32_t staticStartMs  = 0;

// If the fixed address does not produce a connection in this long, DHCP is
// tried instead. Without this, carrying the instrument to any other network
// would leave it permanently offline with an address that cannot route there -
// a worse failure than the one static addressing is here to fix.
constexpr uint32_t STATIC_FALLBACK_MS = 20000;

void applyStatic() {
    IPAddress ip, gw, mask, dns;
    if (!ip.fromString(STATIC_IP) || !gw.fromString(STATIC_GATEWAY) ||
        !mask.fromString(STATIC_SUBNET) || !dns.fromString(STATIC_DNS)) {
        Serial.println("[net] static address is malformed - using DHCP");
        return;
    }
    if (!WiFi.config(ip, gw, mask, dns)) {
        Serial.println("[net] static address rejected - using DHCP");
        return;
    }
    staticActive  = true;
    staticStartMs = millis();
    Serial.printf("[net] static address %s\n", STATIC_IP);
}

void revertToDhcp() {
    Serial.println("[net] static address did not connect - falling back to DHCP");
    fellBackToDhcp = true;
    staticActive   = false;
    WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
    WiFi.disconnect(false);   // drop the session, never power the radio down
    WiFi.begin(WIFI_SSID, WIFI_PASS);
}
#endif

}  // namespace

namespace net {

void begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
#if defined(STATIC_IP)
    applyStatic();
#else
    Serial.println("[net] using DHCP");
#endif
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    startNtp();
}

void loop() {
#if defined(STATIC_IP)
    if (staticActive && !fellBackToDhcp && WiFi.status() != WL_CONNECTED &&
        millis() - staticStartMs > STATIC_FALLBACK_MS) {
        revertToDhcp();
    }
#endif

    // Log the IP once per connection, independent of NTP state below - DHCP
    // can hand out a different lease on any reconnect, and with nothing ever
    // printing it the device becomes a subnet-scan exercise to find. Runs on
    // every loop() call (not just pre-NTP-sync) so a lease change after a
    // WiFi drop/reconnect gets logged too.
    if (!ipLogged && WiFi.status() == WL_CONNECTED) {
        Serial.print("[net] IP address: ");
        Serial.println(WiFi.localIP());
        ipLogged = true;
    } else if (ipLogged && WiFi.status() != WL_CONNECTED) {
        ipLogged = false;   // disconnected - log again on reconnect, lease may differ
    }

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
