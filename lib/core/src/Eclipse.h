#pragma once
#include "Vec3.h"

constexpr double EARTH_RADIUS_KM = 6378.137;

// Cylindrical Earth-shadow test (spec section 5, condition 3). The satellite is
// eclipsed when it lies on the anti-sun side of the Earth AND its perpendicular
// distance from the Earth-Sun axis is less than one Earth radius.
//
// This ignores the penumbra and the Sun's angular size, so it is wrong only in a
// narrow band at the terminator. A conical model is a deliberate non-goal.
bool isSunlit(const Vec3& satEci, const Vec3& sunEci);
