'use strict';

// Logical panel geometry. These must stay 240x240 - the browser is a simulator
// for the GC9A01, not a bigger screen.
const SIZE = 240;
const CX = 120, CY = 120, R = 120;

// Deep-red instrument palette. #d8f4ff is reserved exclusively for
// "visible right now" and must never be used for chrome.
const P = {
  bg:      '#060305',
  gridDim: '#3a1210',
  gridMid: '#5a1c17',
  floor:   '#6b241d',
  text:    '#c9503c',
  visible: '#d8f4ff',
  sweep:   'rgba(201,80,60,0.10)',
  // M4: meteor shower radiants. A distinct deep red/crimson, warmer than the
  // grid but never #d8f4ff - a radiant is a region of sky to watch, not an
  // object visible right now, and must stay visually unmistakable from that.
  radiant:     '#e0293f',
  radiantPeak: '#ff6b57',
};

const cv = document.getElementById('scope');
const g = cv.getContext('2d');

let snap = null;
let heading = 0;      // FixedNorth for now; ManualNorth/Magnetometer at M7
let sweepDeg = 0;

// Poll failure tracking. A transient drop must not blank a working display -
// only after LINK_LOST_THRESHOLD consecutive misses do we degrade the status
// text, and even then the last known blips keep drawing (dimmed).
const LINK_LOST_THRESHOLD = 5;
let consecutiveFailures = 0;

function linkLost() {
  return consecutiveFailures >= LINK_LOST_THRESHOLD;
}

// theta is sky-frame degrees (0 = true north). Canvas 0deg points right, so
// subtract 90 to put north at the top. Heading rotation happens here, in the
// renderer, never in the core.
function polar(r, thetaDeg) {
  const a = (thetaDeg - heading - 90) * Math.PI / 180;
  return [CX + r * R * Math.cos(a), CY + r * R * Math.sin(a)];
}

function ring(rNorm, colour) {
  g.strokeStyle = colour;
  g.lineWidth = 1;
  g.beginPath();
  g.arc(CX, CY, rNorm * R - 0.5, 0, Math.PI * 2);
  g.stroke();
}

// The core computes ring radii from elevation via skyRadius() and tells us
// which ones are the visibility floor / horizon; this only picks a colour
// for each `kind`. No projection math belongs here.
function ringColour(kind) {
  if (kind === 'floor') return P.floor;
  if (kind === 'horizon') return P.gridMid;
  return P.gridDim;
}

function drawChrome() {
  g.fillStyle = P.bg;
  g.fillRect(0, 0, SIZE, SIZE);

  const rings = (snap && snap.rings) ? snap.rings : [];
  for (const rg of rings) {
    ring(rg.r, ringColour(rg.kind));
  }

  g.strokeStyle = P.gridDim;
  for (let th = 0; th < 360; th += 30) {
    const [x1, y1] = polar(0.955, th);
    const [x2, y2] = polar(1.0, th);
    g.beginPath();
    g.moveTo(x1, y1);
    g.lineTo(x2, y2);
    g.stroke();
  }

  g.fillStyle = P.text;
  g.font = '10px ui-monospace, monospace';
  g.textAlign = 'center';
  g.textBaseline = 'middle';
  for (const [label, th] of [['N', 0], ['E', 90], ['S', 180], ['W', 270]]) {
    const [x, y] = polar(0.90, th);
    g.fillText(label, x, y);
  }
}

function drawSweep() {
  const a = (sweepDeg - heading - 90) * Math.PI / 180;
  g.beginPath();
  g.moveTo(CX, CY);
  g.arc(CX, CY, R, a - 0.45, a);
  g.closePath();
  g.fillStyle = P.sweep;
  g.fill();
}

// Blip radius in pixels from apparent magnitude. Brighter (more negative)
// objects draw larger. Clamped so nothing vanishes or blooms.
function blipRadius(mag) {
  if (!isFinite(mag)) return 2;
  return Math.max(1.5, Math.min(4.5, 3.2 - mag * 0.45));
}

// Starlink trains (M3): a fresh batch is still flying in formation, so what
// makes one worth showing is that several blips move together as a line -
// draw a thin connector through them, sorted by elevation radius so it reads
// as the order they cross the sky in, rather than as scattered dots that
// happen to share a colour. Still deep-red/cyan per the existing palette;
// #d8f4ff stays reserved for "visible right now" whether or not the blip is
// a train member.
function drawTrainConnector(members, dim) {
  if (members.length < 2) return;
  const sorted = [...members].sort((a, b) => a.r - b.r);
  g.strokeStyle = P.text;
  g.lineWidth = 1;
  g.globalAlpha = dim * 0.35;
  g.beginPath();
  for (let i = 0; i < sorted.length; i++) {
    const [x, y] = polar(sorted[i].r, sorted[i].theta);
    if (i === 0) g.moveTo(x, y); else g.lineTo(x, y);
  }
  g.stroke();
  g.globalAlpha = 1;
}

// Train members draw as a small diamond rather than a circle - a distinct
// glyph, not a distinct colour, so "visible right now" (#d8f4ff) still reads
// as the one thing that's cyan.
function drawTrainMarker(x, y, radius, colour) {
  g.fillStyle = colour;
  g.beginPath();
  g.moveTo(x, y - radius);
  g.lineTo(x + radius, y);
  g.lineTo(x, y + radius);
  g.lineTo(x - radius, y);
  g.closePath();
  g.fill();
}

function drawBlips() {
  if (!snap || !snap.blips) return;

  // Once the link is lost we keep showing the last known picture rather than
  // erasing it, just dimmed so it reads as stale rather than live.
  const dim = linkLost() ? 0.4 : 1;

  const trainMembers = snap.blips.filter((b) => b.kind === 'train');
  drawTrainConnector(trainMembers, dim);

  for (const b of snap.blips) {
    const colour = b.visible ? P.visible : P.text;
    const isTrain = b.kind === 'train';

    if (b.trail) {
      for (let i = 0; i < b.trail.length; i++) {
        const [tr, tt] = b.trail[i];
        const [x, y] = polar(tr, tt);
        g.globalAlpha = dim * (0.12 + 0.5 * (i / Math.max(1, b.trail.length)));
        g.fillStyle = colour;
        g.beginPath();
        g.arc(x, y, 1, 0, Math.PI * 2);
        g.fill();
      }
      g.globalAlpha = 1;
    }

    const [x, y] = polar(b.r, b.theta);
    g.globalAlpha = dim;
    if (isTrain) {
      drawTrainMarker(x, y, blipRadius(b.mag) + 0.5, colour);
    } else {
      g.fillStyle = colour;
      g.beginPath();
      g.arc(x, y, blipRadius(b.mag), 0, Math.PI * 2);
      g.fill();
    }
    g.globalAlpha = 1;
  }
}

// The panel can only ever show one blip at a time, so when nothing higher-
// priority pre-empts it (NO TIME / OPEN /CONFIG / LINK LOST / an imminent
// event), the status arc rotates through snap.blips roughly every 3 s rather
// than pinning on the top-ranked one forever. cycleIndex only ever advances
// on its own timer below; statusLine()/statusBlip() just read it modulo the
// current blip count, so a shrinking list can't put it out of range.
let cycleIndex = 0;
setInterval(() => {
  cycleIndex++;
  renderRoster();
}, 3000);

// Which blip (if any) the status arc is showing right now. Null whenever the
// arc is showing something other than a blip readout (NO TIME, OPEN /CONFIG,
// LINK LOST, CONNECTING, OFFLINE, NOTHING UP, or an imminent event) - the
// roster has nothing to highlight in that case. Kept in one place so the
// canvas readout and the HTML roster can never disagree about which row is
// "current".
function statusBlip() {
  if (linkLost() || !snap) return null;
  if (snap.status === 'no_time' || snap.status === 'no_location') return null;
  if (snap.events && snap.events.length > 0) return null;
  if (snap.status === 'offline') return null;
  if (!snap.blips || snap.blips.length === 0) return null;
  return snap.blips[cycleIndex % snap.blips.length];
}

// The bottom arc holds roughly 20 characters. Anything longer must be
// shortened in the core, not here.
function statusLine() {
  if (linkLost()) return 'LINK LOST';   // failing long enough to call it dead
  if (!snap) return 'CONNECTING';       // still within the initial grace period
  switch (snap.status) {
    case 'no_time':     return 'NO TIME';
    case 'no_location': return 'OPEN /CONFIG';
  }

  // A pending visible pass takes priority over the current-position readout.
  if (snap.events && snap.events.length > 0) {
    const e = snap.events[0];
    const mins = Math.max(0, Math.round(e.startsIn / 60));
    const when = mins >= 60 ? Math.round(mins / 60) + 'H' : mins + 'M';
    const name = e.name.split(' ')[0].slice(0, 8);
    return (name + ' ' + when + ' ' + Math.round(e.maxEl) + 'DEG').slice(0, 20);
  }

  if (snap.status === 'offline') return 'OFFLINE';
  if (!snap.blips || snap.blips.length === 0) return 'NOTHING UP';

  const b = statusBlip();
  return (b.name.split(' ')[0].slice(0, 10) + ' ' +
          Math.round(90 - b.r * 90) + 'DEG').slice(0, 20);
}

function drawStatus() {
  g.fillStyle = P.text;
  g.font = '10px ui-monospace, monospace';
  g.textAlign = 'center';
  g.textBaseline = 'middle';
  g.fillText(statusLine(), CX, 212);

  if (snap && snap.tleAgeHours >= 0) {
    g.fillText('TLE ' + snap.tleAgeHours.toFixed(0) + 'H', CX, 28);
  }
}

// Radiants (M4) draw as a soft glow rather than a hard dot - a radiant is a
// region of sky worth watching, not an object to point at. Drawn before the
// blips so a satellite dot always reads on top of the glow rather than being
// swallowed by it. Size scales gently with ZHR so the strongest showers
// (Geminids, Perseids) read as a bigger patch of sky than a minor one.
function drawRadiants() {
  if (!snap || !snap.radiants) return;
  const dim = linkLost() ? 0.4 : 1;

  for (const rad of snap.radiants) {
    const [x, y] = polar(rad.r, rad.theta);
    const radius = Math.max(6, Math.min(20, 6 + rad.zhr * 0.08));
    const colour = rad.atPeak ? P.radiantPeak : P.radiant;

    const grad = g.createRadialGradient(x, y, 0, x, y, radius);
    grad.addColorStop(0, colour);
    grad.addColorStop(1, 'rgba(0,0,0,0)');

    g.globalAlpha = dim * (rad.atPeak ? 0.55 : 0.35);
    g.fillStyle = grad;
    g.beginPath();
    g.arc(x, y, radius, 0, Math.PI * 2);
    g.fill();
  }
  g.globalAlpha = 1;
}

function frame() {
  sweepDeg = (sweepDeg + 1.5) % 360;
  drawChrome();
  drawSweep();
  drawRadiants();
  drawBlips();
  drawStatus();
  requestAnimationFrame(frame);
}

// --- Roster: an HTML companion view below the canvas, entirely separate from
// the panel simulation above. Every ordinary blip in the snapshot gets a row
// here even though the panel itself can only ever narrate one of them at a
// time; this is where "SL-14 47DEG" gets a name and the other eleven dots
// stop being anonymous. Starlink train members (M3) are the one exception:
// they collapse into a single "TRAIN <designator> x<N>" row (see
// trainGroupRow() below) instead of one row per near-identical satellite.
// No hover/click handling - it's a plain read-only table.

const COMPASS16 = ['N', 'NNE', 'NE', 'ENE', 'E', 'ESE', 'SE', 'SSE',
                    'S', 'SSW', 'SW', 'WSW', 'W', 'WNW', 'NW', 'NNW'];

// 16-point compass (22.5deg per sector) - the roster has room for the extra
// precision that the 20-char panel arc never could.
function compassPoint(thetaDeg) {
  const t = ((thetaDeg % 360) + 360) % 360;
  return COMPASS16[Math.round(t / 22.5) % 16];
}

const REASON_TEXT = {
  visible:       'Visible',
  below_horizon: 'Below horizon',
  daylight:      'Daylight',
  eclipsed:      'In shadow',
  too_dim:       'Too dim',
};

function reasonText(reason) {
  return REASON_TEXT[reason] || reason;
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, (c) => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;',
  }[c]));
}

// One roster row for an ordinary blip.
function satRow(b, activeId) {
  // mag is the 99.00 sentinel except on the visible/too_dim paths (see
  // Visibility.h) - anything else would just be showing "99" as if it
  // meant something.
  const magKnown = b.visible || b.reason === 'too_dim';
  const magText = magKnown ? b.mag.toFixed(2) : '—';
  const bearing = compassPoint(b.theta) + ' ' +
    Math.round(((b.theta % 360) + 360) % 360);

  const classes = ['roster-row'];
  if (b.visible) classes.push('roster-visible');
  if (b.id === activeId) classes.push('roster-active');

  return '<tr class="' + classes.join(' ') + '">' +
    '<td>' + escapeHtml(b.name) + '</td>' +
    '<td>' + b.el.toFixed(1) + '&deg;</td>' +
    '<td>' + bearing + '</td>' +
    '<td>' + magText + '</td>' +
    '<td>' + reasonText(b.reason) + '</td>' +
    '</tr>';
}

// One roster row for a whole train, grouped under its launch designator
// (e.g. "TRAIN 2026-159") rather than one row per member - a pass can put
// most or all of a batch above the horizon at once, and sixty near-identical
// "STARLINK-xxxxx" rows would bury everything else in the roster.
function trainGroupRow(members, activeId) {
  const first = members[0];
  const designator = first.launchYear + '-' + String(first.launchNumber).padStart(3, '0');
  const els = members.map((m) => m.el);
  const elText = members.length > 1
    ? Math.min(...els).toFixed(1) + '–' + Math.max(...els).toFixed(1) + '&deg;'
    : els[0].toFixed(1) + '&deg;';

  // Bearing of whichever member is currently highest, as a representative
  // heading for the whole formation.
  const highest = members.reduce((a, b) => (a.el >= b.el ? a : b));
  const bearing = compassPoint(highest.theta) + ' ' +
    Math.round(((highest.theta % 360) + 360) % 360);

  const anyVisible = members.some((m) => m.visible);
  const activeInGroup = members.some((m) => m.id === activeId);

  const classes = ['roster-row'];
  if (anyVisible) classes.push('roster-visible');
  if (activeInGroup) classes.push('roster-active');

  return '<tr class="' + classes.join(' ') + '">' +
    '<td>TRAIN ' + escapeHtml(designator) + ' &times;' + members.length + '</td>' +
    '<td>' + elText + '</td>' +
    '<td>' + bearing + '</td>' +
    '<td>—</td>' +
    '<td>' + (anyVisible ? 'Visible' : 'Above horizon') + '</td>' +
    '</tr>';
}

function renderRoster() {
  const body = document.getElementById('roster-body');
  if (!body) return;

  if (!snap || !snap.blips || snap.blips.length === 0) {
    body.innerHTML = '<tr><td class="roster-empty" colspan="5">NOTHING TRACKED</td></tr>';
    return;
  }

  const active = statusBlip();
  const activeId = active ? active.id : null;

  // Group train members by launch designator (year + launch number) so one
  // launch's satellites collapse into a single row; every other blip keeps
  // its own row exactly as before.
  const trainGroups = new Map();
  const rowOrder = [];   // preserves snap.blips' rank order for non-train rows
  for (const b of snap.blips) {
    if (b.kind === 'train') {
      const key = b.launchYear + '-' + b.launchNumber;
      if (!trainGroups.has(key)) {
        trainGroups.set(key, []);
        rowOrder.push({ train: key });
      }
      trainGroups.get(key).push(b);
    } else {
      rowOrder.push({ sat: b });
    }
  }

  let rows = '';
  for (const entry of rowOrder) {
    rows += entry.sat
      ? satRow(entry.sat, activeId)
      : trainGroupRow(trainGroups.get(entry.train), activeId);
  }
  body.innerHTML = rows;
}

// One roster row for an active meteor shower radiant.
function showerRow(r) {
  const bearing = compassPoint(r.theta) + ' ' +
    Math.round(((r.theta % 360) + 360) % 360);

  const classes = ['roster-row'];
  if (r.atPeak) classes.push('shower-peak');

  return '<tr class="' + classes.join(' ') + '">' +
    '<td>' + escapeHtml(r.name) + '</td>' +
    '<td>' + r.el.toFixed(1) + '&deg;</td>' +
    '<td>' + bearing + '</td>' +
    '<td>' + r.zhr + '</td>' +
    '<td>' + (r.atPeak ? 'At peak' : 'Active') + '</td>' +
    '</tr>';
}

function renderShowers() {
  const body = document.getElementById('showers-body');
  if (!body) return;

  if (!snap || !snap.radiants || snap.radiants.length === 0) {
    body.innerHTML = '<tr><td class="roster-empty" colspan="5">NO SHOWER ACTIVE</td></tr>';
    return;
  }

  body.innerHTML = snap.radiants.map(showerRow).join('');
}

async function poll() {
  try {
    const res = await fetch('/api/scope', { cache: 'no-store' });
    snap = await res.json();
    consecutiveFailures = 0;
  } catch (e) {
    // Transient drop: keep the last known snapshot on screen. consecutiveFailures
    // drives the LINK_LOST_THRESHOLD degrade in statusLine()/drawBlips(), snap
    // itself is left untouched (still null on a never-successful first load).
    consecutiveFailures++;
  }
  renderRoster();
  renderShowers();
}

setInterval(poll, 1000);
poll();
requestAnimationFrame(frame);

// The panel has no touch input; this is a simulator-only convenience so the
// setup state is not a dead end in the browser.
cv.addEventListener('click', () => {
  if (snap && snap.status === 'no_location') window.location.href = '/config';
});
