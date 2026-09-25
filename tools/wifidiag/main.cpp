// WiFi diagnostic - not part of the firmware.
//
// The board associates with nothing and the production firmware says nothing
// about why: net::loop() logs only on success, so "still trying" and "failing
// repeatedly" look identical from the serial console. That gap is what has kept
// this guessy.
//
// The ESP32 emits a reason code on every disconnect. That single number
// separates a wrong password from a full access point from a DHCP problem, and
// it has been available all along - nothing was listening for it.
//
// A previous version of this file called WiFi.disconnect(true) before scanning.
// That powers the radio down, which made a healthy board look stuck in "idle".
// It is deliberately absent here.
//
// Second attempt uses a fixed address. If association succeeds and only the
// address assignment fails, a static IP will come straight up - which would
// confirm the router's DHCP as the culprit rather than anything on the board.

#include <Arduino.h>
#include <WiFi.h>

#include "secrets.h"

static const char* reasonName(uint8_t r) {
    switch (r) {
        case 1:   return "UNSPECIFIED";
        case 2:   return "AUTH_EXPIRE - auth timed out";
        case 3:   return "AUTH_LEAVE";
        case 4:   return "ASSOC_EXPIRE";
        case 5:   return "ASSOC_TOOMANY - access point is full";
        case 6:   return "NOT_AUTHED";
        case 7:   return "NOT_ASSOCED";
        case 8:   return "ASSOC_LEAVE - AP kicked us";
        case 15:  return "4WAY_HANDSHAKE_TIMEOUT - usually a wrong password";
        case 200: return "BEACON_TIMEOUT";
        case 201: return "NO_AP_FOUND";
        case 202: return "AUTH_FAIL - password rejected";
        case 203: return "ASSOC_FAIL";
        case 204: return "HANDSHAKE_TIMEOUT";
        case 205: return "CONNECTION_FAIL";
        default:  return "(see esp_wifi_types.h)";
    }
}

static void onEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println("  [evt] station started");
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.printf("  [evt] ASSOCIATED to the access point (channel %d)\n",
                          info.wifi_sta_connected.channel);
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("  [evt] GOT IP: ");
            Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            Serial.println("  [evt] lost IP");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
            const uint8_t r = info.wifi_sta_disconnected.reason;
            Serial.printf("  [evt] DISCONNECTED  reason %u - %s\n", r, reasonName(r));
            break;
        }
        default:
            break;
    }
}

static void attempt(const char* label, bool useStatic) {
    Serial.println();
    Serial.printf("--- %s ---------------------------\n", label);

    WiFi.disconnect(false);      // drop the session, DO NOT power the radio down
    delay(400);

    if (useStatic) {
        IPAddress ip(192, 168, 31, 77);
        IPAddress gw(192, 168, 31, 1);
        IPAddress mask(255, 255, 255, 0);
        IPAddress dns(192, 168, 31, 1);
        if (!WiFi.config(ip, gw, mask, dns)) {
            Serial.println("  WiFi.config() rejected");
        } else {
            Serial.println("  using fixed address 192.168.31.77");
        }
    } else {
        // 0.0.0.0 everywhere puts DHCP back in charge.
        WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
        Serial.println("  using DHCP");
    }

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    const uint32_t start = millis();
    while (millis() - start < 25000) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("  RESULT: connected, IP ");
            Serial.print(WiFi.localIP());
            Serial.printf(", RSSI %d dBm\n", WiFi.RSSI());
            return;
        }
        delay(200);
    }
    Serial.println("  RESULT: no connection after 25 s");
}

void setup() {
    Serial.begin(115200);
    delay(700);
    Serial.println();
    Serial.println("====================================================");
    Serial.println(" WIFI DIAGNOSTIC v2 - with reason codes");
    Serial.println("====================================================");
    Serial.printf(" SSID : \"%s\"\n", WIFI_SSID);

    WiFi.onEvent(onEvent);
    WiFi.mode(WIFI_STA);
    delay(300);
    Serial.print(" MAC  : ");
    Serial.println(WiFi.macAddress());

    attempt("ATTEMPT 1: DHCP", false);
    attempt("ATTEMPT 2: STATIC 192.168.31.77", true);

    Serial.println();
    Serial.println("====================================================");
}

void loop() { delay(10000); }
