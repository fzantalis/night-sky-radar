#pragma once

// A parsed two-line element set. Raw lines are retained verbatim because SGP4
// re-parses them itself; the derived fields exist so callers never have to.
struct Tle {
    char   name[25]                = {0};
    int    satnum                  = 0;
    int    launchYear              = 0;   // 4-digit, from the COSPAR designator
    int    launchNumber            = 0;   // launch of that year
    double inclinationDeg          = 0.0;
    double meanMotionRevPerDay     = 0.0;
    char   line1[70]               = {0};
    char   line2[70]               = {0};
};

// Validates the mod-10 checksum in column 69. Digits count as their value,
// '-' counts as 1, everything else counts as 0.
bool tleChecksumValid(const char* line);

// Returns false and leaves `out` untouched if either line is malformed, has a
// bad checksum, carries the wrong line number, or the two lines disagree about
// which satellite they describe.
bool parseTle(const char* name, const char* l1, const char* l2, Tle& out);
