# SGP4 — provenance

- **Source:** github.com/dnwrnr/sgp4
- **Commit / tag retrieved:** `661e057a5d369d5ee424676cf1d69cbead95ff2c` (HEAD of `main` at time of retrieval)
- **Date retrieved:** 2026-09-02
- **Licence:** Apache-2.0 (LICENSE retained at `lib/core/src/sgp4/LICENSE`). The upstream
  repository does not ship a separate `NOTICE` file, so none is vendored — there is
  nothing to retain beyond the LICENSE text itself.
- **Files vendored** (all from `libsgp4/` in the upstream repo, placed flat under
  `lib/core/src/sgp4/`):
  - `SGP4.h`, `SGP4.cc` — the propagator core
  - `Tle.h`, `Tle.cc` — libsgp4's own TLE parser (distinct from this project's `Tle`)
  - `OrbitalElements.h`, `OrbitalElements.cc`
  - `Eci.h`, `Eci.cc`
  - `DateTime.h` (header-only)
  - `TimeSpan.h` (header-only)
  - `Vector.h`, `Vector.cc`
  - `Util.h`, `Util.cc`
  - `Globals.h`, `Globals.cc`
  - `SatelliteException.h`, `SatelliteException.cc`
  - `DecayedException.h`, `DecayedException.cc`
  - `TleException.h`, `TleException.cc`
  - `CoordGeodetic.h`, `CoordGeodetic.cc` — **not** in the brief's minimum list, but
    vendored because `Eci.h` (a required file) `#include`s `CoordGeodetic.h` directly.
    Per the brief's own instruction ("if a needed file includes one, vendor it rather
    than editing the include"), this was vendored rather than editing the upstream
    `Eci.h`. `CoordGeodetic.{h,cc}` itself only depends on `Util.h` — it pulls in none
    of `Observer.*`, `CoordTopocentric.*`, or `SolarPosition.*`, so the spirit of "omit
    the topocentric/observer machinery, this project has its own" is preserved.
  - `LICENSE`
  - Explicitly **not** vendored: `Observer.*`, `CoordTopocentric.*`, `SolarPosition.*`,
    `CsvTleLoader.*`, `CMakeLists.txt`, and everything under the upstream `test/`,
    `sample/`, `libcsv/` directories. This project has its own topocentric/observer
    code (from earlier tasks) and does not need libsgp4's CSV loader or examples.
- **Local modifications:** none. All vendored files are byte-for-byte copies of the
  upstream sources; only the copyright/licence headers already present in each file
  were retained as-is.

## Why not Vallado's reference implementation

CelesTrak now distributes it under AGPL-3.0, whose section 13 network clause
would require the whole project to be AGPL on publication. Verification is
still done against Vallado's published test vectors, which are numerical data.
