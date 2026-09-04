#include "Projection.h"

double skyRadius(double elDeg) {
    double r = (90.0 - elDeg) / 90.0;
    if (r < 0.0) r = 0.0;
    if (r > 1.0) r = 1.0;
    return r;
}
