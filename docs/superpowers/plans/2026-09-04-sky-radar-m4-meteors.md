# M4 — Meteor shower radiants

**Date:** 2026-09-04
**Prerequisite:** M3 merged (168 native tests).

**Goal:** show, on the same dial, where tonight's meteor shower radiant sits and whether the
shower is active — the one class of "space object worth going outside for" that has no
ephemeris at all.

**Process:** one dispatch, hardware-verified. Review focused on the almanac data itself,
because that is the only part that can be silently wrong.

---

## Why this is not satellite tracking

There is no per-meteor ephemeris and never will be. What is real and predictable is the
**shower**: a fixed radiant position on the celestial sphere, an activity window of dates,
a peak, and a rate (ZHR — zenithal hourly rate). That is almanac data, not orbital
mechanics. No network, no TLEs, no propagation.

So a radiant is a **fixed point in equatorial coordinates** (right ascension and
declination) that rises and sets like a star. Converting it to the dial's azimuth and
elevation is the only maths involved.

## Task 1: RA/Dec to horizontal coordinates

New `lib/core/src/Celestial.h/.cpp`:

```cpp
// Converts a fixed equatorial position (right ascension, declination, both
// degrees, J2000) to the observer's horizontal frame at a given instant.
// Radiants are fixed stars for this purpose - no propagation involved.
LookAngles radiantLookAngles(double raDeg, double decDeg,
                             const Observer& obs, int64_t unixSeconds);
```

Standard transform, using the project's existing `timeutils::gmstDegrees`:

- local sidereal time `LST = GMST + observer longitude`
- hour angle `H = LST - RA`
- `sin(el) = sin(dec)sin(lat) + cos(dec)cos(lat)cos(H)`
- `az = atan2(-sin(H)cos(dec), cos(lat)sin(dec) - sin(lat)cos(dec)cos(H))`, normalised to
  `[0,360)` measured from north increasing eastward

**Clamp the `asin` argument to `[-1,1]` before calling it.** This project has already been
bitten once by an unclamped `asin` producing NaN at the zenith, and `Topocentric.cpp` and
`Magnitude.cpp` both carry that guard now. Match them.

**Tests must pin the convention from both ends**, the way `test_magnitude`'s
backlit/fully-lit pair does. A test that passes under either sign convention is worthless —
this project has shipped an inverted phase angle exactly that way. At minimum:

- An object at the observer's zenith (declination = latitude, hour angle 0) is at elevation
  90 regardless of longitude or time.
- The celestial pole (dec = +90) sits due north at elevation equal to the observer's
  latitude, for any time — a pure symmetry check with no algebra.
- From the southern hemisphere the same pole is *below* the horizon.
- Azimuth is east of north for an object rising, west for one setting.

## Task 2: The almanac

New `lib/core/src/MeteorShowers.h/.cpp` — a static table:

```cpp
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
```

**SOURCE THE DATA, DO NOT WRITE IT FROM MEMORY.** Use the International Meteor
Organization's published shower calendar (imo.net) or the IAU Meteor Data Center as the
citable source, and record in `docs/third-party/METEOR-ALMANAC-PROVENANCE.md` exactly where
each figure came from and when you retrieved it. This project has already shipped a wrong
TLE checksum and a physically impossible tolerance because I wrote constants from memory;
do not repeat that. If you cannot reach a citable source, say so and stop rather than
inventing plausible numbers.

Cover at least the major annual showers — Quadrantids, Lyrids, Eta Aquariids, Perseids,
Orionids, Leonids, Geminids, Ursids. Include the activity window, not just the peak; a
shower is worth showing for days either side.

Provide:

```cpp
// Showers active on the given date, soonest-peaking first. Handles windows
// that cross the new year.
std::vector<const MeteorShower*> activeShowers(int month, int day);
```

The year-crossing case is a real trap — the Quadrantids run from late December into early
January. Test it explicitly.

## Task 3: Put radiants on the dial

- Add a `radiants` array to `Snapshot`: name, `r`, `theta` (same normalised polar
  convention as blips), elevation, ZHR, and whether the shower is at peak.
- Include a radiant only when the shower is active **and** the radiant is above the horizon
  **and** the sun is below `MAX_SUN_ALT_DEG` — meteors are invisible in daylight, and a
  radiant below the horizon means the shower is not observable from here yet.
- Draw radiants distinctly from satellites: a soft glow or sector rather than a hard dot,
  since a radiant is a region of sky to watch rather than an object to point at. Deep-red
  palette; `#d8f4ff` stays reserved for "satellite visible right now".
- The roster gains a section listing active showers with their ZHR.

## Done criteria

1. `radiantLookAngles` passes symmetry tests that would fail under an inverted convention.
2. The almanac's provenance document names a real source and a retrieval date for every
   figure.
3. On device: report which showers the almanac considers active today (2026-09-04) and
   where their radiants sit. **If none are active, that is a correct result** — early
   September is genuinely quiet between the Perseids and the Orionids — and must be reported
   as a pass, not worked around by widening a window.
4. Existing satellite tracking, pass prediction and the LED are unaffected.
5. Native tests still pass, plus the new coverage.
