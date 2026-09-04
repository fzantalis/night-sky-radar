# M3 — Starlink train detection

**Date:** 2026-09-04
**Prerequisite:** M2 merged (153 native tests, master at `d8aa2d8`).

**Goal:** show recently-launched Starlink "trains" — the tight strings of satellites that
*are* naked-eye visible for a week or so after launch — without drowning the dial in the
~8,000 operational Starlinks that are not.

**Process note:** M3–M5 run lighter than M0–M2. One task per feature, hardware-verified,
with review focused on the parts that touch the network or persistent storage.

---

## Spec correction — the original filter was not implementable

The design spec says: *"stream-filter the ~8000-object `starlink` group by COSPAR launch
designator, keeping only launches from the last ~14 days."*

**A COSPAR designator gives the launch YEAR and the Nth launch of that year — not a date.**
`25123A` is the 123rd launch of 2025; nothing in it says when that was. So "the last 14
days" cannot be derived from the designator alone.

**The physically correct discriminator is mean motion.** A freshly deployed batch sits at
roughly 350 km and is still raising orbit; operational Starlinks sit at ~550 km. Mean
motion follows directly:

| altitude | mean motion (rev/day) |
|---|---|
| ~350 km (fresh deployment) | ~15.6 |
| ~450 km (raising) | ~15.3 |
| ~550 km (operational) | ~15.06 |

This is also the *right* filter conceptually: what makes a train visible is that it is low
and tightly clustered, and low is exactly what mean motion measures. The launch designator
is still useful as a secondary grouping key — members of one train share it — so keep using
the `launchYear`/`launchNumber` that `parseTle` already extracts.

**Filter:** `meanMotionRevPerDay >= 15.35` AND the object's `launchYear` is the current year
or the previous one. Tune the threshold against real data during implementation and report
what it actually selects.

---

## Task 1: A genuinely streaming fetch

`tlefetcher::fetchGroup()` calls `http.getString()`, which materialises the entire response
body in one Arduino `String`. For `stations` (~4 KB) that is fine. For `starlink` it is
about **1.7 MB** against roughly 240 KB of free internal heap. It cannot work.

A comment in `TleFetcher.cpp` currently claims the parser streams and that this "is what
makes the 1.2 MB Starlink group tractable at M3". **That claim is false today** — the
parser holds three lines at a time, but only after the whole body is already in memory. A
reviewer flagged it during M0–M1. Delete or correct it.

**Add** (do not replace the existing function — `stations` and `visual` are small and the
current path works and is well tested):

```cpp
// Streams a CelesTrak group without ever holding the whole body. Reads the
// socket line by line, assembles one three-line element set at a time, parses
// it, and offers it to `keep`. Only accepted entries are retained.
//
// Returns the number of entries accepted, or -1 on transport failure.
using TleFilter = bool (*)(const Tle&);
int fetchGroupStreaming(const char* group, TleFilter keep,
                        std::vector<Tle>& out, int maxKeep);
```

Requirements:

- Read from `http.getStream()` with a bounded per-line buffer. Never accumulate the body.
- Reuse the same 5-root TLS trust store and the same serialised-fetch discipline — never
  two concurrent handshakes.
- Malformed triples are skipped, not fatal — `parseTle` already validates the checksum.
- Respect `maxKeep` so a pathological response cannot exhaust memory.
- Apply the existing parse-before-commit discipline: this feeds `tracked` directly, so
  validate the accepted count before disturbing anything.
- **Report peak internal heap during the fetch.** That is the number that proves it streams.

## Task 2: Wire trains into the scope

- Add the filter described above and fetch the `starlink` group on the same 12-hour cadence,
  after `stations` and `visual`, serialised.
- Cap the number of train members kept — a fresh batch is ~20–60 objects; 80 is a safe cap.
- Cache them like the other groups (group-aware `TleStore`, temp-and-rename).
- Give train members a distinguishable identity in the snapshot so the renderer can draw
  them as a group rather than as unrelated dots — add a `kind` field to `Blip`
  (`"sat"` / `"train"`), defaulting to `"sat"`.

## Task 3: Draw them as a train

In `data/radar.js`, render train members with a visually distinct treatment — they move
together in a line, and that is the whole reason they are worth showing. Keep the deep-red
palette; `#d8f4ff` stays reserved for "visible right now".

The roster should group train members under their launch designator rather than listing
sixty near-identical rows.

## Done criteria

1. The `starlink` group fetch completes on device without exhausting internal heap, with
   the peak figure reported.
2. The filter selects a plausible number of train members — report how many, their launch
   designators, and their mean motions, and sanity-check that against recent launches.
3. If no recent launch exists at the time of testing, the correct result is **zero train
   members**, and that must be reported as a pass, not worked around.
4. Existing `stations` and `visual` tracking is unaffected.
5. Native tests still pass, plus coverage for the filter predicate itself.
