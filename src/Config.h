#pragma once
#include "Observer.h"

namespace config {

void begin();

// False until a location has been stored. The scope must report
// ScopeStatus::NoLocation rather than assume a default position.
bool hasLocation();

Observer observer();
void setObserver(const Observer& obs);

}  // namespace config
