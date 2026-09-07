#pragma once

// Shared value-noise / fbm / Inigo Quilez domain-warp primitives. Factored
// out of noise_field once image_warp needed the exact same math, rather
// than duplicating it.
namespace domain_noise {

float fbm(float x, float y, int octaves);
float warpedNoise(float x, float y);

}  // namespace domain_noise
