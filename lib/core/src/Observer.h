#pragma once

// Geodetic position of the ground station. East longitude is positive.
struct Observer {
    double latDeg = 0.0;
    double lonDeg = 0.0;
    double altKm  = 0.0;
};
