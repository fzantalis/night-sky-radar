# Meteor shower almanac — provenance

- **Source:** International Meteor Organization, *2026 Meteor Shower Calendar*
  (IMO_INFO(3-25)), edited by Jürgen Rendtel. This is the IMO's own annual
  citable publication (see M4 plan's requirement for "the IMO's published
  shower calendar (imo.net) or the IAU Meteor Data Center").
- **URL:** https://www.imo.net/files/meteor-shower/cal2026.pdf (linked from
  https://www.imo.net/resources/calendar/ and
  https://www.imo.net/the-2026-imo-meteor-shower-calendar-is-here/).
- **DOI:** 10.13140/RG.2.2.36179.08480
- **Copyright:** © International Meteor Organization, 2025.
- **Date retrieved:** 2026-09-04. Fetched via `WebFetch`; the tool returned the
  raw PDF (965 KB) rather than an extracted summary because the content is a
  scanned/typeset PDF, so the actual PDF bytes were read directly (Claude's
  PDF reader) and the table below was copied from that rendering, not from any
  intermediate summarisation.
- **Table used:** Table 5, "Working List of Visual Meteor Showers" (PDF page
  25 of 28), which the Calendar itself describes as "the single most accurate
  listing available anywhere today for visual meteor observing," with "maximum
  dates accurate only for 2026."

## Figures imported into `lib/core/src/MeteorShowers.cpp`

`lib/core/src/MeteorShowers.cpp` implements only the eight showers the M4 plan
names explicitly ("Cover at least the major annual showers — Quadrantids,
Lyrids, Eta Aquariids, Perseids, Orionids, Leonids, Geminids, Ursids"), not
IMO's full ~38-row working list (which also includes minor/daytime showers
such as the Antihelion Source, the Aurigids, the September ε-Perseids, etc.).
Every figure below is copied verbatim from Table 5:

| Shower (IAU code)         | Activity          | Peak (2026) | Radiant α    | Radiant δ | ZHR |
|----------------------------|-------------------|-------------|--------------|-----------|-----|
| Quadrantids (010 QUA)      | Dec 28 – Jan 12   | Jan 03      | 230°         | +49°      | 80  |
| April Lyrids (006 LYR)     | Apr 14 – Apr 30   | Apr 22      | 271°         | +34°      | 18  |
| η-Aquariids (031 ETA)      | Apr 19 – May 28   | May 06      | 338°         | −01°      | 50  |
| Perseids (007 PER)         | Jul 17 – Aug 24   | Aug 13      | 48°          | +58°      | 100 |
| Orionids (008 ORI)         | Oct 02 – Nov 07   | Oct 21      | 95°          | +16°      | 20  |
| Leonids (013 LEO)          | Nov 06 – Nov 30   | Nov 17      | 152°         | +22°      | 15  |
| Geminids (004 GEM)         | Dec 04 – Dec 20   | Dec 14      | 112°         | +33°      | 150 |
| Ursids (015 URS)           | Dec 17 – Dec 26   | Dec 22      | 217°         | +76°      | 10  |

α/δ are equinox-2000.0 (J2000) coordinates, per the Calendar's own "Abbreviations
and symbols" section (page 24: "All λ⊙ are given for the equinox 2000.0",
and α/δ are defined immediately above that as the radiant coordinates). ZHR
figures are the Calendar's own "recent observed returns" column, not a
theoretical maximum.

`MeteorShower::zhr` stores the peak ZHR only (an `int`, matching the struct in
the plan); the Calendar's variability notes (e.g. Lyrids "can be variable, up
to 90", Orionids "20+") are not encoded — only the point figure in Table 5's
ZHR column is used, since the struct has no field for a range.

## Deliberately not imported

- The Antihelion Source (ANT) and other minor/daytime showers from the same
  Table 5 — out of scope per the plan's explicit shower list above.
- Per-shower radiant drift (Table 6) — the plan's `MeteorShower` struct has a
  single fixed radiant position, not a date-indexed drift table. Table 6 shows
  drift is real (e.g. the Quadrantid radiant moves ~4°/day near its peak) but
  small over the multi-day activity window this milestone renders; adding
  drift is a future enhancement, not part of M4's scope.
- Velocity (`V∞`) and population index (`r`) — not part of the `MeteorShower`
  struct requested by the plan, and not needed for the dial (position,
  active-window, and ZHR are the only things drawn).

## Cross-check against the Aurigids (transparency note, not a table entry)

While reading Table 5 in full to extract the eight required rows, one nearby
row is worth flagging even though it is out of scope: the **Aurigids (206
AUR)**, activity **Aug 28 – Sep 05**, are technically active on the milestone's
report date (2026-09-04) per IMO's full working list — one day before their
window closes. Because M4's almanac deliberately covers only the eight majors
named in the plan, this shower is not in `MeteorShowers.cpp` and does not
appear in the on-device roster. This is a scope choice, not an error: with the
eight-shower almanac the plan specifies, zero showers active on 2026-09-04 is
still the correct result (see the M4 report for confirmation on hardware).
