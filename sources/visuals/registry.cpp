#include "registry.h"
#include "noise_field.h"
#include "kaleidoscope.h"

namespace visuals {

namespace {

const Visual kVisuals[] = {
    {"Noise Field", noise_field::init, noise_field::update, noise_field::deinit, nullptr},
    {"Kaleidoscope", kaleidoscope::init, kaleidoscope::update, kaleidoscope::deinit, kaleidoscope::onTouch},
};

}  // namespace

const Visual* all(int* count) {
    *count = sizeof(kVisuals) / sizeof(kVisuals[0]);
    return kVisuals;
}

}  // namespace visuals
