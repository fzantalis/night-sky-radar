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

function drawBlips() {
  if (!snap || !snap.blips) return;

  // Once the link is lost we keep showing the last known picture rather than
  // erasing it, just dimmed so it reads as stale rather than live.
  const dim = linkLost() ? 0.4 : 1;

  for (const b of snap.blips) {
    const colour = b.visible ? P.visible : P.text;

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
    g.fillStyle = colour;
    g.globalAlpha = dim;
    g.beginPath();
    g.arc(x, y, blipRadius(b.mag), 0, Math.PI * 2);
    g.fill();
    g.globalAlpha = 1;
  }
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

  const b = snap.blips[0];
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

function frame() {
  sweepDeg = (sweepDeg + 1.5) % 360;
  drawChrome();
  drawSweep();
  drawBlips();
  drawStatus();
  requestAnimationFrame(frame);
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
}

setInterval(poll, 1000);
poll();
requestAnimationFrame(frame);

// The panel has no touch input; this is a simulator-only convenience so the
// setup state is not a dead end in the browser.
cv.addEventListener('click', () => {
  if (snap && snap.status === 'no_location') window.location.href = '/config';
});
