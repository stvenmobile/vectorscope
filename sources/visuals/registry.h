#pragma once

#include "visual.h"

// The full list of visuals main.cpp can cycle through. To add a new one:
// write its init/update(/deinit), then add one line in registry.cpp.
namespace visuals {

const Visual* all(int* count);

}  // namespace visuals
