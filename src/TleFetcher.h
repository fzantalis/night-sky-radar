#pragma once
#include <Arduino.h>
#include <vector>
#include "Tle.h"

namespace tlefetcher {

// Blocking HTTPS fetch of one CelesTrak group. The TLS client is fully torn
// down before returning - never hold one open, never run two concurrently.
bool fetchGroup(const char* group, String& outRaw);

}  // namespace tlefetcher

// Parses CelesTrak three-line format. Malformed or bad-checksum triples are
// skipped rather than aborting the whole parse. Returns the number parsed.
int parseTleText(const String& raw, std::vector<Tle>& out, int maxCount);
