# Vallado SGP4 — provenance

**STATUS: NOT VENDORED. Licence gate failed — see "Gate decision" below.**

`lib/core/src/sgp4/SGP4.h` and `SGP4.cpp` do **not** exist in this repository.
No vendoring was performed. This document records what was investigated and
why the task stopped at Step 1.

## What was checked

`celestrak.org/software/vallado-sw.php` no longer hosts a standalone C++ zip
with its own licence note (the historical `sgp4ext.cpp` / `sgp4unit.cpp` /
`sgp4io.cpp` distribution). The page now points to a GitHub repository as the
sole distribution channel for the companion code to *Fundamentals of
Astrodynamics and Applications*:

> "Code and data associated with the textbook are available at the project's
> GitHub repository: https://github.com/CelesTrak/fundamentals-of-astrodynamics"

That repository contains the current C++ SGP4 source at
`software/cpp/SGP4/SGP4/SGP4.h` and `software/cpp/SGP4/SGP4/SGP4.cpp`, fetched
directly via their raw GitHub URLs (not the historical celestrak.org zip, which
no longer appears to be published separately).

- **Source URL (page pointing to distribution):** https://celestrak.org/software/vallado-sw.php
- **Source URL (actual C++ files):**
  - https://raw.githubusercontent.com/CelesTrak/fundamentals-of-astrodynamics/main/software/cpp/SGP4/SGP4/SGP4.h
  - https://raw.githubusercontent.com/CelesTrak/fundamentals-of-astrodynamics/main/software/cpp/SGP4/SGP4/SGP4.cpp
- **Repository:** https://github.com/CelesTrak/fundamentals-of-astrodynamics
- **Date retrieved:** 2026-09-02
- **Version / date stamp in the header comment (verbatim, from `SGP4.h`):**
  ```
  #define SGP4Version  "SGP4 Version 2025-12-30"
  ```
  The changelog block at the top of `SGP4.h`/`SGP4.cpp` (companion code for
  *Fundamentals of Astrodynamics and Applications*, 2022, David Vallado) lists
  its most recent entry as:
  ```
  current :
            30 dec 25  david vallado
                         add alpha5 support
  ```

## Licence terms as stated by the distributor (verbatim)

The `SGP4.h` / `SGP4.cpp` files themselves carry **no licence statement in
their header comment** — only the author/version changelog quoted above (I
grepped both files for `licen`, `copyright`, `permission`, `agpl`, `gpl`:
zero matches). The licence governing the files is stated at the repository
level, in two places that agree with each other:

**1. Repository `README.md`, "License" section (verbatim):**

> ## License
> This code is released under the [GNU Affero General Public License
> v3.0](./LICENSE). You are free to use, modify, and distribute it under the
> terms of this license, provided that any modifications or derivative works
> are also made available under the same license.

**2. Repository root `LICENSE` file** — the full, unmodified text of the GNU
Affero General Public License, Version 3, 19 November 2007. Opening lines
(verbatim):

> GNU AFFERO GENERAL PUBLIC LICENSE
> Version 3, 19 November 2007
>
> Copyright (C) 2007 Free Software Foundation, Inc. <https://fsf.org/>
> Everyone is permitted to copy and distribute verbatim copies of this
> license document, but changing it is not allowed.

The operative clause most relevant to this project (embedded firmware that
may expose functionality over a network) is Section 13 (verbatim):

> 13. Remote Network Interaction; Use with the GNU General Public License.
>
> Notwithstanding any other provision of this License, if you modify the
> Program, your modified version must prominently offer all users interacting
> with it remotely through a computer network (if your version supports such
> interaction) an opportunity to receive the Corresponding Source of your
> version by providing access to the Corresponding Source from a network
> server at no charge, through some standard or customary means of
> facilitating copying of software. This Corresponding Source shall include
> the Corresponding Source for any work covered by version 3 of the GNU
> General Public License that is incorporated pursuant to the following
> paragraph.

The README also states, in the "Future Plans and C++ Updates" section
(verbatim), that the C++ port is not the project's primary target going
forward:

> We have included C++, but a full update is planned in the coming months. We
> have heard concerns about C++ memory leaks making it vulnerable to
> exploitation, so its' long-term role in this project may have some
> uncertainties.

## Gate decision: BLOCKED

Per the task brief's hard gate: *"if the licence does not clearly permit
inclusion, do not proceed — flag it and stop."*

The AGPL-3.0 does grant broad rights to copy, modify, and redistribute the
code — so "permission to include a copy in a repository" is not in question.
What is in question, and is a decision only the project owner can make, is
whether the AGPL-3.0's conditions are acceptable for this project:

1. **Copyleft propagation.** AGPL-3.0 is a "strong" copyleft licence. Combining
   `SGP4.cpp`/`SGP4.h` with the rest of `esp_sot` to build a single firmware
   image very likely makes the combined work a derivative/combined work that
   must itself be distributed under AGPL-3.0 terms (source available, same
   licence) once the firmware is distributed to anyone (e.g. flashed to a
   device that leaves this machine).
2. **Network source-disclosure (§13, quoted above).** If the ESP32-S3 device
   exposes any functionality to users over a network (Wi-Fi web UI, API,
   OTA server, etc. — plausible for a "sky radar" device), the AGPL requires
   offering those users the Corresponding Source of the running version, not
   merely the upstream Vallado source.
3. No alternative, differently-licensed distribution of this exact reference
   implementation was located during this investigation. (No substitute
   implementation was sought or vendored — the brief explicitly prohibits
   that.)

**No files were vendored under `lib/core/src/sgp4/`. No `Propagator.h`,
`Propagator.cpp`, or `test/test_propagator/test_propagator.cpp` were created.
No commit was made.** This document itself, recording the investigation, is
the only artifact produced for Task 4.

## Local modifications

None — no source was vendored, so no portability edits were made or needed.
(For reference, a scan of the downloaded `SGP4.cpp`/`SGP4.h` suggests the
brief's anticipated portability fixes — `M_PI`, narrowing conversions,
`char*`/`const char*` — would likely still apply if vendoring is later
approved: `SGP4.cpp` defines its own `pi` macro rather than relying on
`M_PI`, and both files are plain ISO C++ with no Arduino/ESP-IDF dependency,
so they should be compilable under `platform = native` given the usual
20-year-old-C++ portability nursing. This was not verified by an actual
compile, since Step 2 was not reached.)
