# Night Sky Radar

A standalone desk instrument that shows which satellites are overhead right
now — and, more usefully, which of them you could actually *see* with the naked
eye if you walked outside.

No cloud service and no phone app. The device fetches orbital elements over
WiFi and propagates them itself with SGP4, so once it has data it keeps working
offline.

Two LEDs count down to the next visible pass, so you don't have to watch the
screen. Wave your hand to switch between satellites and asteroids.

---

## What it shows

**SKY mode** — a map of the sky above you. The centre is straight overhead, the
rim is the horizon, and north is up. A dot's size is its brightness; a pale
blue dot means *visible right now, go outside*. Meteor shower radiants appear as
soft patches when a shower is active and the sky is dark.

Only four conditions together make a satellite visible, and the device checks
all of them: it is more than 10° above the horizon, the sun is more than 6°
below yours, the satellite is still in sunlight, and it is brighter than
magnitude +4.5.

**NEO mode** — asteroid close approaches from NASA/JPL. Both axes change
meaning: the radius is the miss distance (the rim is 10 lunar distances), and
the angle is *when* it happens across a 30-day window. The ring at 1 lunar
distance is the Moon's orbit — anything inside it passes closer than the Moon.

**The LEDs** — one lit means a visible pass within 10 minutes, both mean within
5, and the pulse quickens as it approaches. They work in both modes.

---

## Parts

| Part | Notes |
|---|---|
| ESP32-S3-DevKitC-1 | ESP32-S3-WROOM-1 **N16R8** (16 MB flash, 8 MB PSRAM). The PSRAM is required — the framebuffer alone is 253 KB |
| 2.1" round TFT, 360×360 | **GC9B72** controller, 4-wire SPI |
| PAJ7620U2 gesture sensor | I²C, address `0x73` |
| 2 × WS2812 | Cut from an addressable strip |
| USB-C cable | Powers and flashes the board |

The `enclosure*.stl` files and the FreeCAD source are in the repository root.

---

## Wiring

> **Both the display and the gesture sensor are 3.3 V only.** 5 V on either
> will destroy it. Neither breakout has a regulator.

> **Never use GPIO 26–37 on an N16R8.** They are wired to the SPI flash and the
> octal PSRAM. 35/36/37 are broken out on the header, which makes them an easy
> trap.

**Display (GC9B72)**

| Display | ESP32-S3 |
|---|---|
| `VCC` | **3V3** |
| `GND` | GND |
| `SCL` | GPIO 12 |
| `SDA` | GPIO 11 |
| `CS` | GPIO 10 |
| `DC` | GPIO 13 |
| `RST` | GPIO 14 |
| `BL` | GPIO 9 |
| `SDO`, `TE` | leave unconnected |

`CS`/`SDA`/`SCL` are the S3's native SPI pins. Keep these leads short — the
panel is good for about 20 MHz on jumper wires.

**Gesture sensor (PAJ7620U2)**

| Sensor | ESP32-S3 |
|---|---|
| `VCC` | **3V3** |
| `GND` | GND |
| `SDA` | GPIO 17 |
| `SCL` | GPIO 18 |
| `INT` | GPIO 16 |

**LEDs (2 × WS2812)**

| Strip | ESP32-S3 |
|---|---|
| `+5V` | **3V3**, not 5V — see below |
| `GND` | GND |
| `DIN` | GPIO 15 |

The strip runs from 3.3 V deliberately. A WS2812 needs a logic high above
0.7 × VDD; on a 5 V supply that is 3.5 V, which an ESP32 GPIO cannot reach, so
5 V operation is marginal. At 3.3 V the threshold is 2.31 V and the same pin
clears it comfortably. It is slightly under the part's nominal minimum, which
costs nothing at the low brightness this instrument uses.

The strip is directional — wire the `DIN` end, not `DO`.

All pin assignments live in [`src/Pins.h`](src/Pins.h).

---

## Quick start

You need [PlatformIO](https://platformio.org/).

```bash
git clone https://github.com/fzantalis/night-sky-radar.git
cd night-sky-radar
cp src/secrets.h.example src/secrets.h     # then fill in your WiFi
pio run -t upload                          # firmware
pio run -t uploadfs                        # web UI files
```

On first boot the panel shows `SET LOCATION`. Find the device's IP in the
serial log (115200 baud), then open `http://<ip>/config` and enter your
latitude, longitude and altitude.

`http://<ip>/` gives the same dial in a browser — useful before you have the
panel wired, and it draws from the identical data the panel does.

**Controls.** Swipe left/right to change mode, up/down for brightness, wave to
wake the screen. In the browser, the arrow keys and space do the same. The BOOT
button also switches mode, as a fallback when no gesture sensor is attached.

---

## Tests

```bash
pio test -e native
```

224 tests covering the orbital mathematics, visibility rules, pass prediction
and data parsing. They run on the host, with no board attached — everything in
`lib/core/` is deliberately free of Arduino and ESP-IDF headers so it can be
tested that way.

---

## Data sources

| Source | Used for |
|---|---|
| [CelesTrak](https://celestrak.org/) | Orbital elements (TLE) |
| [NASA/JPL SBDB](https://ssd-api.jpl.nasa.gov/doc/cad.html) | Asteroid close approaches |
| [IMO](https://www.imo.net/) | Meteor shower almanac |

Provenance for every constant and data source, including the magnitude and
diameter relations, is recorded in [`docs/third-party/`](docs/third-party/).

---

## Licence

MIT — see [LICENSE](LICENSE).

This project includes a vendored copy of the
[dnwrnr/sgp4](https://github.com/dnwrnr/sgp4) library under the Apache License
2.0. See [NOTICE](NOTICE) and
[`lib/core/src/sgp4/LICENSE`](lib/core/src/sgp4/LICENSE).
