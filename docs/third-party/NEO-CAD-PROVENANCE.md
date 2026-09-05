# NEO close-approach data — provenance

Everything M5 draws comes from one API and three constants. This records where
each came from and when, because this project has already shipped a wrong TLE
checksum and a physically impossible tolerance that were written from memory.

## Data source

**NASA/JPL Solar System Dynamics — SBDB Close Approach Data (CAD) API**

- Documentation: <https://ssd-api.jpl.nasa.gov/doc/cad.html>
- Endpoint: `https://ssd-api.jpl.nasa.gov/cad.api`
- Retrieved and verified: **2026-09-05**
- No API key required, and no rate limit is documented.

Request the firmware makes (`src/NeoService.cpp`):

```
https://ssd-api.jpl.nasa.gov/cad.api?dist-max=10LD&date-min=now&date-max=%2B30&sort=date&fullname=true
```

`dist-max` accepts lunar distances directly as `10LD`, and `date-max` accepts a
relative `+30` (URL-encoded `%2B30`), so the dial's own parameters map onto the
query without any client-side date arithmetic.

The whole 30-day response was **1,249 bytes** when measured on 2026-09-05, with
6 approaches. That is why this fetch uses `getString()` rather than the
streaming path the 1.7 MB Starlink group needs.

### Response shapes that had to be handled

Three distinct shapes, all confirmed against the live API on 2026-09-05:

| Case | Body | Handling |
|---|---|---|
| Results | `{"signature":…,"count":N,"fields":[…],"data":[[…]]}` | parsed |
| **No results** | `{"count":0,"signature":{…}}` — **no `fields`, no `data`** | success, empty list |
| Error | `{"moreInfo":…,"code":"400","message":…}` with HTTP 400 | rejected |

The zero-result case is the trap. A parser that requires `fields` and `data`
would report a genuinely quiet month as a corrupt payload, which is
indistinguishable from a network fault — the display would be pinned to a stale
list with no way to tell. `test_zero_count_response_is_a_success_not_a_failure`
pins this against the captured body.

Unlike CelesTrak, which serves plain-text errors as HTTP 200, this API reports
its own errors with a real status code, so a non-200 here is conclusive.

### TLS

`ssd-api.jpl.nasa.gov` chains:

```
ssd-api.jpl.nasa.gov
  -> Entrust OV TLS Issuing RSA CA 2   (Entrust Limited)
  -> Sectigo Public Server Authentication Root R46   (Sectigo Limited)
```

Verified with `openssl s_client -showcerts` on 2026-09-05. That root is already
the **first** certificate in the existing multi-root trust store in
`src/TleFetcher.cpp`, so M5 needed no new root — `tlefetcher::trustedRootsPem()`
is shared rather than a second copy being embedded.

## Constants

### Lunar distance

`neo::AU_PER_LD = 384400.0 / 149597870.7`

- 384,400 km — the conventional mean Earth–Moon distance.
- 149,597,870.7 km — the astronomical unit, IAU 2012 Resolution B2 definition
  (exact, by definition).

The CAD API reports distances in au and the dial is scaled in lunar distances,
so every conversion goes through this one constant.
`test_one_lunar_distance_round_trips` pins it from the data side.

### Julian date epoch

`2440587.5` is the Julian date of 1970-01-01T00:00:00Z, used by
`neo::julianDateToUnix()`.

The API's `jd` field is TDB, which runs roughly 70 s ahead of UTC. That is
ignored deliberately: the dial spans 30 days across 360 degrees, so one degree
is about two hours, and the offset is far below one pixel.

`test_parsed_time_agrees_with_the_apis_own_calendar_string` cross-checks the
converted `jd` against the API's own `cd` calendar string in the same row — two
independent representations of one instant, so a wrong epoch or an off-by-one
column index disagrees.

### Diameter from absolute magnitude

`Snapshot::estimatedDiameterMetres()` uses:

```
D(km) = 1329 / sqrt(albedo) * 10^(-H/5)
```

**Source:** NASA/JPL CNEOS Asteroid Size Estimator,
<https://cneos.jpl.nasa.gov/tools/ast_size_est.html>, retrieved 2026-09-05,
which states the relation as:

> `d = 10^[ 3.1236 - 0.5 log10(a) - 0.2H ]`

These are the same expression, verified term by term:

- `10^3.1236 = 1329.0` (log10 1329 = 3.12352)
- `10^(-0.5 log10 a) = a^(-1/2) = 1 / sqrt(a)`
- `10^(-0.2H) = 10^(-H/5)`

**Albedo 0.14** is the conventional assumption for an asteroid with no measured
albedo. CNEOS deliberately recommends no default and warns that a wrong albedo
can be off by nearly a factor of two, so this figure is presented as an
order-of-magnitude estimate, never as a measurement. It is worth carrying
anyway: "about 11 m" communicates far more on a dial than "H = 27.56".

Worked check, reproduced by `test_diameter_estimate_matches_the_standard_relation`:

| H | albedo | D |
|---|---|---|
| 27.56 (real 2026 RG) | 0.14 | 10.9 m |
| 17.75 (the classic 1 km threshold) | 0.14 | ~1,000 m |

## On-device verification, 2026-09-05

Fetched live and rendered on the ESP32-S3. Six approaches within 10 lunar
distances over 30 days; the nearest was 2026 RG at 1.32 LD, about 11 m across,
4.8 hours out. Its projected angle was 2.40 degrees, which matches
`17269 s / 2592000 s * 360` exactly, and the furthest at 25.6 days projected to
307 degrees — the time axis behaving as specified across the whole window.
