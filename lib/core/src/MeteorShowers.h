#pragma once

#include <vector>

// A fixed annual meteor shower. See
// docs/third-party/METEOR-ALMANAC-PROVENANCE.md for exactly where every
// figure below came from and when it was retrieved - do not add or edit an
// entry without updating that document too.
struct MeteorShower {
    const char* name;
    double raDeg;        // radiant, J2000
    double decDeg;
    int    peakMonth;    // 1-12
    int    peakDay;
    int    startMonth, startDay;   // activity window
    int    endMonth,   endDay;
    int    zhr;          // zenithal hourly rate at peak
};

// Showers active on the given date (month 1-12, day 1-31), soonest-peaking
// first. Handles activity windows that cross the new year (e.g. the
// Quadrantids run from late December into early January) - a shower is
// active if `date` falls within [start, end] going forward from start,
// wrapping past December 31 into January when end < start.
std::vector<const MeteorShower*> activeShowers(int month, int day);
