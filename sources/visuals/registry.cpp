#include "registry.h"
#include "noise_field.h"
#include "kaleidoscope.h"
#include "image_warp.h"

namespace visuals {

namespace {

// First entry is the default visual on boot.
const Visual kVisuals[] = {
    {"Image Warp", image_warp::init, image_warp::update, image_warp::deinit, image_warp::onTouch},
    {"Noise Field", noise_field::init, noise_field::update, noise_field::deinit, nullptr},
    {"Kaleidoscope", kaleidoscope::init, kaleidoscope::update, kaleidoscope::deinit, kaleidoscope::onTouch},
};

}  // namespace

const Visual* all(int* count) {
    *count = sizeof(kVisuals) / sizeof(kVisuals[0]);
    return kVisuals;
}

}  // namespace visuals
