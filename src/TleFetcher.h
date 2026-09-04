#pragma once
#include <Arduino.h>
#include <vector>
#include "Tle.h"

namespace tlefetcher {

// Blocking HTTPS fetch of one CelesTrak group. The TLS client is fully torn
// down before returning - never hold one open, never run two concurrently.
bool fetchGroup(const char* group, String& outRaw);

// Predicate a caller supplies to fetchGroupStreaming(). A plain function
// pointer, not std::function, so a caller with runtime parameters (a
// threshold, a "current year") stashes them in file-local statics and reads
// them from inside a fixed-signature wrapper - see ScopeService.cpp.
using TleFilter = bool (*)(const Tle&);

// Streams a CelesTrak group without ever holding the whole body in memory:
// reads the socket line by line into a small fixed buffer, assembles one
// three-line element set at a time, parses it, and offers it to `keep`.
// Only entries `keep` accepts are appended to `out`, up to `maxKeep` - this
// is what makes the ~1.7 MB `starlink` group (~240 KB free internal heap)
// tractable, where fetchGroup()'s single-String slurp cannot work.
//
// The stream is always drained to completion (or transport failure),
// regardless of whether `out` has already reached `maxKeep` - stopping early
// would leave the TLS connection in a state HTTPClient can't cleanly tear
// down, and `totalParsed` (the count of every triple that parsed and
// checksummed cleanly, before `keep` ever runs) needs the whole body to be a
// meaningful "did we actually get the real catalog" signal - see
// tlecount::tleCountIsPlausible, which this is meant to feed.
//
// Malformed triples are skipped, not fatal. Returns the number of entries
// accepted into `out`, or -1 on transport failure (HTTP begin/GET failure -
// never for "zero matched the filter", which is a legitimate result).
// `totalParsed` and `minFreeHeapOut`, if non-null, receive the raw parsed-
// triple count and the lowest ESP.getFreeHeap() observed during the fetch.
int fetchGroupStreaming(const char* group, TleFilter keep, std::vector<Tle>& out,
                        int maxKeep, int* totalParsed = nullptr,
                        uint32_t* minFreeHeapOut = nullptr);

}  // namespace tlefetcher

// Parses CelesTrak three-line format. Malformed or bad-checksum triples are
// skipped rather than aborting the whole parse. Returns the number parsed.
int parseTleText(const String& raw, std::vector<Tle>& out, int maxCount);
