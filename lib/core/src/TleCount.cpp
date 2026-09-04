#include "TleCount.h"

namespace tlecount {

bool tleCountIsPlausible(int count, int previousCount) {
    if (count <= 0) return false;
    if (previousCount <= 0) return true;   // no history yet: any non-empty parse is fine
    return count * 2 >= previousCount;
}

}  // namespace tlecount
