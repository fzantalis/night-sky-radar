#pragma once

// Equidistant azimuthal sky projection: zenith at the centre, horizon at the
// rim. Values are clamped, so an object below the horizon renders on the rim
// rather than outside the dial.
double skyRadius(double elDeg);
