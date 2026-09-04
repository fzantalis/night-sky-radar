#pragma once
#include <cstdint>

namespace net {

void begin();
void loop();

bool wifiUp();

// True once NTP has succeeded at least once. Stays true across WiFi drops
// because the RTC keeps running. Nothing may propagate while this is false.
bool timeValid();

int64_t nowUnix();

}  // namespace net
