#include "Eclipse.h"

bool isSunlit(const Vec3& satEci, const Vec3& sunEci) {
    const Vec3 sunHat = sunEci.unit();

    // Degenerate sun vector: fail open rather than declaring a permanent
    // eclipse, which would silently hide every object.
    if (sunHat.norm() <= 0.0) return true;

    const double along = satEci.dot(sunHat);

    // On the sunward side of the terminator plane: always lit.
    if (along > 0.0) return true;

    // Perpendicular distance from the Earth-Sun axis.
    const Vec3 perp = satEci - sunHat * along;
    return perp.norm() >= EARTH_RADIUS_KM;
}
