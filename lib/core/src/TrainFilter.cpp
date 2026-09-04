#include "TrainFilter.h"

#include <cstring>

namespace trainfilter {

bool isTrainCandidate(const Tle& t, int currentYear, const Params& params) {
    if (t.meanMotionRevPerDay < params.minMeanMotionRevPerDay) return false;
    if (t.launchYear != currentYear && t.launchYear != currentYear - 1) return false;
    // Substring match, not an exact tag compare: CelesTrak's "[DTC]" suffix
    // is stable enough in practice, and a substring test survives whatever
    // whitespace/bracket variation a future feed revision introduces.
    if (std::strstr(t.name, "DTC") != nullptr) return false;
    return true;
}

}  // namespace trainfilter
