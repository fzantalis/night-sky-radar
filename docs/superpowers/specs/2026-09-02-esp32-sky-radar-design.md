# ESP32-S3 Sky Radar — Design

**Date:** 2026-09-02
**Status:** Approved for implementation planning
**Scope of this spec:** Milestones M0–M2. M3–M7 are sketched for architectural context only.

## 1. Purpose

A standalone desk instrument that shows what is above you in near-Earth space on a circular radar-style display, with emphasis on objects you could actually walk outside and see with the naked eye.

The device is standalone: it fetches orbital elements a couple of times a day and does all propagation, visibility determination and projection itself. It keeps working through a router reboot. It is not a thin client for a tracking service.

Hardware target is an ESP32-S3-DevKitC-1 N16R8 (16 MB flash, 8 MB octal PSRAM). The final display is a GC9A01 240x240 round IPS SPI panel, not yet purchased. Until it arrives, the same renderer output is served to a browser as a 240x240 panel simulator.

## 2. Scope decisions

### In scope for M0–M2

- ISS and Tiangong tracking (CelesTrak `stations` group)
- The ~160 brightest catalogued objects (CelesTrak `visual` group)
- On-device SGP4 propagation
- Naked-eye visibility determination
- Forward pass prediction with countdowns
- A 240x240 circular web renderer that obeys the physical panel's constraints
- Native (PC) unit tests for the whole orbital/visibility core

### Deferred, but the architecture must not preclude them

- **M3** Starlink train detection — stream-filter the ~8000-object `starlink` group by COSPAR launch designator, keeping only launches from the last ~14 days
- **M4** Meteor shower radiants — static almanac, rendered as a glowing sector
- **M5** NEO mode — JPL CAD API, Earth-centred, 10 lunar distances at the rim, time-scrubbed over 30 days rather than rendered live
- **M6** GC9A01 renderer
- **M7** NeoPixel bezel ring, rotary encoder, magnetometer, captive-portal config

### Explicitly rejected

- **N2YO as a runtime data source.** Its REST API is well built, but sourcing positions from it makes the device a thin client that goes blank when WiFi hiccups, and binds the project to a free tier's rate limits. It is used instead as a **development-time test oracle** (see section 10).
- **Tracking all visible satellites server-side.** Loses the standalone quality that is the point of the project.
- **Meteors as tracked objects.** There is no per-meteor ephemeris. Only showers (radiant, peak date, ZHR) are real, and those are almanac data.
- **Starlink as a general naked-eye target.** Operational Starlinks with visors sit at roughly magnitude 5.5–7 and are not naked-eye objects. Only recently launched, still-clustered trains are worth displaying.

## 3. Architecture

### 3.1 The central decision

**Projection happens in the core; renderers are dumb polar plotters.**

The core computes each object's normalised polar coordinate — `r` in 0..1 and `theta` in degrees — and renderers only draw at (r, theta). Renderers contain no orbital geometry.

Consequences:

- The GC9A01 renderer is a small drawing routine, not a second implementation of the physics.
- SKY mode and NEO mode are the same renderer fed different projections, rather than two separate screens.
- The web view and the panel cannot drift apart, because neither contains anything that could drift.

### 3.2 The snapshot contract

The snapshot is the sole interface between core and renderers.

```json
{
  "t": 1756800000,
  "mode": "sky",
  "sun": { "alt": -12.4, "phase": "nautical" },
  "tleAgeHours": 6.2,
  "status": "ok",
  "blips": [
    { "id": 25544, "name": "ISS", "kind": "sat",
      "r": 0.58, "theta": 142.3,
      "mag": -3.1, "vis": "visible",
      "trail": [[0.61,138.0],[0.60,140.1]] }
  ],
  "events": [
    { "kind": "pass", "name": "ISS", "startsIn": 420,
      "maxEl": 67, "vis": "visible" }
  ]
}
```

`theta` is always in the **sky frame** (0 = true north). Rotation to the device's physical orientation is applied by the renderer at draw time from `HeadingSource`. This is what allows a magnetometer to be added at M7 without touching any maths.

### 3.3 Layers

**`core/` — pure C++, no Arduino or ESP-IDF headers.** TLE parsing (including COSPAR international designator extraction, needed at M3), SGP4 propagation, observer/topocentric geometry, solar position, the visibility engine, blip ranking, and projection into snapshots. Compiles natively on a PC.

**`firmware/` — everything platform-bound.** WiFi, NTP, the streaming CelesTrak fetcher, LittleFS TLE cache, `HeadingSource`, the HTTP/WebSocket server, and renderer implementations.

**`web/` — static assets served from LittleFS.** A 240x240 canvas panel simulator consuming the snapshot over WebSocket. Remains useful as a debug window after the panel arrives.

### 3.4 North as an interface

The renderer only ever calls `screenHeading()`. Three implementations:

- `FixedNorth` — always 0 degrees; used by the web simulator and during bring-up
- `ManualNorth` — a stored offset set once via the rotary encoder (M7)
- `MagnetometerNorth` — live compass heading (M7)

This makes the magnetometer a swappable upgrade rather than a dependency.

### 3.5 Toolchain

PlatformIO with two environments: `esp32-s3-devkitc-1` (Arduino framework) for `firmware/`, and `native` for `core/` tests.

SGP4: vendor **Vallado's reference implementation** rather than an Arduino-specific wrapper. It is the canonical implementation, is plain C++ with no platform dependencies, and drops directly into the natively-testable core.

## 4. Data sources

### 4.1 Orbital elements

```
GET celestrak.org/NORAD/elements/gp.php?GROUP=stations&FORMAT=tle   (M1)
GET celestrak.org/NORAD/elements/gp.php?GROUP=visual&FORMAT=tle     (M2, ~22 KB)
```

No API key, no account. Group queries map directly onto the project's scope.

**TLS:** use the ESP32 Arduino core's bundled Mozilla root store (`setCACertBundle` / `esp_crt_bundle`). Do not use `setInsecure()`, and do not pin a single root certificate — pinning means the device fails permanently when the CA rotates.

**Cadence:** every 12 hours, with exponential backoff on failure (1, 2, 4 … 60 minutes). ISS element sets are republished several times a day after reboosts, but 12 hours is well inside the accuracy budget.

**Caching:** write to LittleFS on every successful fetch, together with the fetch timestamp. A cold boot with no network must still draw a sky from cache.

### 4.2 Time

NTP, UTC. The ISS moves at 7.66 km/s, so one second of clock error is on the order of half a degree of sky — invisible at 240 px. NTP's typical tens of milliseconds is comfortably sufficient.

**If NTP has never succeeded, the device must refuse to propagate** and display an explicit error state rather than plotting from an unknown clock.

### 4.3 Observer position

Latitude, longitude and altitude stored in NVS, editable from the web configuration page. No GPS module. If unset, the device enters the setup state rather than assuming a default location.

## 5. The visibility engine

An object is treated as naked-eye visible when all four conditions hold:

1. **Above horizon** — elevation greater than 10 degrees. Below that, atmospheric extinction and ground obstructions make the claim unreliable.
2. **Observer in darkness** — solar altitude below −6 degrees (end of civil twilight).
3. **Satellite illuminated** — cylindrical Earth-shadow test: project the satellite's ECI position onto the Earth–Sun axis; it is eclipsed if it lies on the anti-sun side and its perpendicular distance from that axis is less than Earth's radius. A conical umbra/penumbra model is a later refinement for terminator-adjacent cases.
4. **Bright enough** — estimated apparent magnitude brighter than about +4.5.

### 5.1 Magnitude estimation

Apparent magnitude follows the standard Molczan/SeeSat-lineage relation, of the form `mag = stdMag − 15.75 + 2.5 · log10(range² / phaseFactor)`, where `phaseFactor` derives from the Sun–satellite–observer phase angle.

The exact constants and phase-factor form are to be pinned down against reference sources and locked in by unit test during implementation (see section 13).

### 5.2 Standard magnitudes

TLE data contains orbits, not brightnesses. `stdMag` values come from separately maintained magnitude files (the McCants/Molczan lineage).

**Approach:** bundle a static `stdMag` table in flash covering the `visual` group, with a conservative default for unrecognised NORAD IDs. This accepts a small maintenance debt in exchange for having no runtime dependency.

## 6. Pass prediction

The `events` list requires forward search: coarse SGP4 stepping to find horizon crossings, then bisection refinement for rise, maximum and set times.

Naively this is roughly 160 objects × 24 hours at 30-second steps, about 460,000 SGP4 evaluations — tens of seconds of continuous compute. This cannot run on the render path.

**Design:**

- Runs as a background task pinned to core 1 at low priority, with explicit yields so it can never starve the render loop or trip the task watchdog.
- Re-runs every 30–60 minutes; results are cached.
- A cheap geometric pre-filter discards objects whose inclination cannot bring them above the observer's horizon, before any propagation occurs.

The foreground loop performs only ~160 single-point SGP4 evaluations per snapshot at 1 Hz, which is negligible.

## 7. Renderer and viewport contract

### 7.1 The contract

Both renderers target a **240x240 logical canvas, circular, RGB565**. The web version displays this scaled 2–3x with `image-rendering: pixelated`, so the browser shows the real aliasing and real colour banding of the panel.

- **No hover, no tooltips, no mouse.** Input is a single rotary encoder: rotate and press. The web page binds arrow keys and Enter only.
- **Text is scarce.** At a legible ~10 px bitmap font, a bottom arc holds roughly 20 characters. UI strings must fit that budget, e.g. `ISS 7m 67`.
- **No smooth gradients.** 16-bit colour bands visibly; use flat fills and dithered rings.
- **Frame budget:** 240×240×2 = 115 KB per frame. A full SPI push at 40 MHz is ~23 ms, giving ~30 fps with DMA. Target **15 fps**.

### 7.2 SKY mode layout

Zenith at centre, horizon at the rim, `r = (90 − el) / 90` (equidistant azimuthal projection), so an overhead pass visibly dives toward the centre and back out.

- Rim circle at elevation 0
- Brighter **visibility floor ring** at elevation 10 degrees (r = 0.89)
- Faint rings at 30 and 60 degrees
- N/E/S/W ticks at the rim, rotating with `HeadingSource`
- Blips sized by magnitude, with a 3–5 point motion trail
- A low-alpha rotating sweep line; purely aesthetic, drawn on a sprite
- Bottom arc showing the next visible pass: name, countdown, maximum elevation

### 7.3 Blip cap

160 objects on a 240 px circle is unreadable. A hard cap of **~12 blips** applies, ranked by: currently visible, then brightness, then elevation. **The core performs this ranking**; the snapshot carries only drawable blips. Renderers never decide what matters.

### 7.4 Palette

A deep-red instrument palette. The device is looked at in darkness immediately before going outside, and long-wavelength red preserves rod dark-adaptation where the traditional phosphor green would bleach it.

- Ground: near-black
- Grid and rings: dim oxide red
- Text: brighter red
- **White/cyan reserved exclusively for "visible right now"**, so the single fact that matters is the only element that is not red

The NeoPixel bezel ring (M7) mirrors the same logic.

## 8. Failure behaviour

Failure states are **screen states**, not log lines. A device with no keyboard must explain itself on the dial.

| Condition | Behaviour |
|---|---|
| WiFi down, cached TLEs present | Continue propagating; dim "offline" mark on rim |
| NTP never succeeded | Refuse to propagate; blank sky and `NO TIME` |
| TLE fetch failed | Exponential backoff; continue serving cache |
| TLE age > 7 days | Amber age readout; still drawing, but declared degraded |
| First boot, no cache, no WiFi | Setup state: SoftAP and configuration page |
| Observer location unset | Setup state; do not assume a default position |

Governing rule: **the screen never shows a confident answer it cannot back.** Stale beats blank; stale-and-labelled beats stale-and-silent.

## 9. Memory and stability

TLS handshakes require roughly 40–50 KB of *internal* heap, which `WiFiClientSecure` does not draw from PSRAM. Concurrent HTTPS fetches, or a long-lived TLS client held open alongside the WebSocket server, are the standard cause of ESP32 projects that reboot daily for no visible reason.

**Rules:**

- Fetches are serialised, short-lived and fully torn down: open, stream-parse, close, free. Never two concurrently, never held open between fetches.
- The framebuffer and parsed TLE store live in PSRAM, away from the network stack.
- The pass-prediction task yields explicitly and is pinned to core 1 at low priority.

## 10. Testing

Three independent checks, all running natively on a PC with no flash cycle:

1. **SGP4 against Vallado's published verification vectors.** Known element sets, known expected state vectors. Passing this proves the propagator is correct, not merely plausible.
2. **Solar position and topocentric conversion** against known values for a fixed date and site.
3. **Full visibility verdicts diffed against N2YO `/visualpasses`** for the ISS at the observer's coordinates over five days. Pass start and end times should agree within a minute or two, and maximum elevation within a degree. This is the check that catches an inverted phase angle or a sign error in the shadow test.

## 11. Repository layout

```
core/            pure C++ — SGP4, geometry, solar, visibility, projection
  test/          native unit tests
firmware/        WiFi, NTP, fetchers, LittleFS cache, HeadingSource,
                 web server, renderers
web/             240x240 panel simulator, served from LittleFS
docs/superpowers/specs/
platformio.ini   two environments: esp32-s3-devkitc-1 (N16R8) and native
```

WiFi credentials live in a gitignored `secrets.h`. A captive portal is the correct long-term answer but belongs in M7.

## 12. Electrical and safety

**M0–M2 carry no electrical risk: nothing is wired.** The board runs bare on USB with no peripherals.

The electrical design — 3.3 V logic limits for the GC9A01, ESP32-S3 strapping pins (GPIO 0, 3, 45, 46), per-GPIO current limits, NeoPixel inrush and its magnetic interference with the magnetometer, and the power budget — is specified when M6/M7 are planned, once the specific panel and LED ring are chosen. Writing it before the parts are known would be guesswork.

Two hardware notes recorded now so they are not forgotten at purchase time:

- Modules sold as **HMC5883L are almost always QMC5883L** — a different manufacturer, different I2C address (0x0D rather than 0x1E) and a different register map. Buy knowingly, or choose a LIS3MDL or MMC5983MA instead.
- A NeoPixel ring encircling a magnetometer is an electromagnet. The sensor needs physical separation, and calibration must be performed in situ with the display and LEDs running.

## 13. Open items to verify during implementation

These are verification tasks, not undecided design questions:

1. Confirm the current availability and licence of Vallado's reference SGP4 source before vendoring it.
2. Pin down the exact apparent-magnitude formula constants and phase-factor form against reference sources; lock in by unit test.
3. Confirm the current source and licence of the McCants/Molczan standard magnitude table before bundling it.
4. Confirm magnetic declination for the observer's actual location at M7 time.

## 14. Milestones

| Milestone | Content | New hardware needed |
|---|---|---|
| M0 | WiFi, NTP, web server, empty radar with sweep | none |
| M1 | ISS and Tiangong; SGP4, topocentric, snapshot; native tests | none |
| M2 | Visibility engine, pass prediction, `visual` group; N2YO validation | none |
| M3 | Starlink train stream-filter | none |
| M4 | Meteor shower radiants | none |
| M5 | NEO mode with time scrub | none |
| M6 | GC9A01 renderer | round panel |
| M7 | NeoPixel ring, encoder, magnetometer, captive portal | ring, encoder, compass |

**This spec covers M0–M2.** M3 onward get their own specs.
