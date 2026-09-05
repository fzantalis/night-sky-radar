#include "MeteorShowers.h"

#include <algorithm>

namespace {

// Cumulative day count at the *start* of each month, non-leap year. Good
// enough here: no shower's activity window touches February 29, and this is
// only used to order/compare (month, day) pairs within a year, never to
// compute an actual calendar date.
constexpr int CUM_DAYS[13] = {
    0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334,
};

int dayOfYear(int month, int day) {
    return CUM_DAYS[month] + day;   // 1 = Jan 1, 366 = Dec 31 (leap slack unused)
}

constexpr int DAYS_IN_YEAR = 366;   // includes a spare day so wraparound math never lands exactly on 0

// Table 5, "Working List of Visual Meteor Showers", IMO 2026 Meteor Shower
// Calendar (IMO_INFO(3-25)). See
// docs/third-party/METEOR-ALMANAC-PROVENANCE.md for source, retrieval date,
// and exactly which columns were copied.
constexpr MeteorShower SHOWERS[] = {
    // name,             raDeg, decDeg, peakM, peakD, startM, startD, endM, endD, zhr
    {"Quadrantids",       230.0,  49.0,  1,  3, 12, 28,  1, 12,  80},
    {"April Lyrids",      271.0,  34.0,  4, 22,  4, 14,  4, 30,  18},
    {"Eta Aquariids",     338.0,  -1.0,  5,  6,  4, 19,  5, 28,  50},
    {"Perseids",           48.0,  58.0,  8, 13,  7, 17,  8, 24, 100},
    {"Orionids",           95.0,  16.0, 10, 21, 10,  2, 11,  7,  20},
    {"Leonids",           152.0,  22.0, 11, 17, 11,  6, 11, 30,  15},
    {"Geminids",          112.0,  33.0, 12, 14, 12,  4, 12, 20, 150},
    {"Ursids",            217.0,  76.0, 12, 22, 12, 17, 12, 26,  10},
};

bool isActive(const MeteorShower& s, int dateOrd) {
    const int start = dayOfYear(s.startMonth, s.startDay);
    const int end   = dayOfYear(s.endMonth, s.endDay);
    if (start <= end) return dateOrd >= start && dateOrd <= end;
    // Window crosses the new year (e.g. the Quadrantids: late Dec into early
    // Jan) - active if on-or-after the start OR on-or-before the end.
    return dateOrd >= start || dateOrd <= end;
}

}  // namespace

std::vector<const MeteorShower*> activeShowers(int month, int day) {
    const int dateOrd = dayOfYear(month, day);

    std::vector<const MeteorShower*> out;
    for (const MeteorShower& s : SHOWERS) {
        if (isActive(s, dateOrd)) out.push_back(&s);
    }

    // Soonest-peaking first: days from `date` forward to the peak, wrapping
    // past year end. A shower whose peak is today sorts first (0); one whose
    // peak already passed within a still-active window sorts toward the back
    // (its "days until peak" wraps almost all the way around).
    std::stable_sort(out.begin(), out.end(),
        [dateOrd](const MeteorShower* a, const MeteorShower* b) {
            const int peakA = dayOfYear(a->peakMonth, a->peakDay);
            const int peakB = dayOfYear(b->peakMonth, b->peakDay);
            auto daysUntil = [dateOrd](int peakOrd) {
                int d = (peakOrd - dateOrd) % DAYS_IN_YEAR;
                if (d < 0) d += DAYS_IN_YEAR;
                return d;
            };
            return daysUntil(peakA) < daysUntil(peakB);
        });

    return out;
}
