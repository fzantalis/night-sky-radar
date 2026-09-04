#pragma once
#include <Arduino.h>
#include <cstdint>

namespace tlestore {

void begin();

bool    save(const char* group, const String& raw);
String  load(const char* group);

// Records a successful fetch. Call this ONLY after save() returned true, so a
// failed fetch never refreshes the reported age. This is the only writer of the
// timestamp - never open the "tlestore" NVS namespace from anywhere else.
void    markFetched(int64_t nowUnix);

// 0 if no successful fetch has ever been recorded.
int64_t lastFetchUnix();

// Hours since the last successful fetch, or -1.0 if never fetched.
double  ageHours(int64_t nowUnix);

}  // namespace tlestore
