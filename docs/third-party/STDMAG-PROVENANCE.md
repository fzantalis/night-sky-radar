# Standard magnitude table — provenance

- **Source URL:** not obtained. Investigated https://mmccants.org/tles/ (index page
  listing `quicksat.mag`, `mcnames.zip`, and related files) and
  https://www.mmccants.org/tles/intrmagdef.html (definition of intrinsic
  magnitude). Neither page publishes a direct download link with an
  accompanying licence for the magnitude file itself, and `mcnames.zip` is
  noted elsewhere as no longer being updated / no longer hosted.
- **Date retrieved:** 2026-09-04 (web search and page fetch only; no data file
  downloaded).
- **Licence / usage terms as stated:** unstated. Neither the McCants TLE index
  page nor the intrinsic-magnitude-definitions page carries a licence,
  copyright notice, or terms-of-use statement for the magnitude data. In the
  absence of any stated terms, the data's redistribution/reuse rights cannot
  be established.
- **Coverage:** 0 NORAD ids imported from this source. The seed table in
  `lib/core/src/StdMagTable.cpp` (6 entries) uses widely published standard
  magnitude values that appear independently across multiple public
  observation/tracking references, not values copied from the McCants or
  Molczan files.

## Gate decision

Per the task brief's gate: **licensing is unclear, so the file was not
imported.** No `quicksat.mag` or `mcnames` data was downloaded or vendored.
This is a deliberate decision, not an oversight — the two source definitions
(McCants "full phase, brightest" vs. Molczan "90°, average") also differ from
each other by ~1.4 magnitudes, which means blindly importing one over the
other would silently pick a philosophy as well as a value; that is a call
better made once licensing permits inspecting the actual file and its
documentation together.

Useful finding for the record: this project's `apparentMagnitude()` formula is
normalised so that `stdMag` is defined at 1000 km range and 90° phase — i.e.
it follows the **Molczan** convention, not the McCants/Quicksat one. Should a
licensed source be obtained later, only Molczan-convention values (or values
explicitly converted to that convention) may be imported into
`StdMagTable.cpp` without also changing `Magnitude.cpp`'s normalisation.

## Default for unknown objects

`DEFAULT_STD_MAG = 2.5`.

Rationale: the only objects reaching `stdMagFor` are members of CelesTrak's
`visual` group, which is *defined* as objects visible to the naked eye. A
mid-range value is therefore a defensible prior for an unlisted member, where a
faint default (+5) would hide the entire group and a bright one (-2) would
promise passes that are not there.

This is spec §13 item 3. If no licensed source is available, the seed table
below stands and the default carries the rest.

## Seed table (not vendored from any third-party file)

| NORAD ID | Object                          | Std. mag |
|----------|----------------------------------|----------|
| 25544    | ISS (ZARYA)                     | -1.8     |
| 48274    | CSS (TIANHE) — Chinese station core | -1.0 |
| 20580    | Hubble Space Telescope          | 2.0      |
| 27386    | Envisat                         | 3.5      |
| 25861    | Okean O                         | 3.0      |
| 23405    | SL-16 rocket body                | 2.5      |

These are commonly cited approximate values (e.g. ISS at roughly magnitude
-1.8 near 1000 km/90°) used here as a small seed, not as a transcription of
any single copyrighted table.
