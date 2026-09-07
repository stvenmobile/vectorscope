#include "domain_noise.h"
#include <cmath>
#include <cstdint>

namespace domain_noise {

namespace {

uint32_t hash2(int x, int y) {
    uint32_t h = (uint32_t)(x * 374761393 + y * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return h;
}

float hashf(int x, int y) {
    return (hash2(x, y) & 0xFFFFFF) / (float)0xFFFFFF;
}

float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

float valueNoise(float x, float y) {
    int x0 = (int)floorf(x), y0 = (int)floorf(y);
    int x1 = x0 + 1, y1 = y0 + 1;
    float sx = smoothstep(x - x0), sy = smoothstep(y - y0);
    float n00 = hashf(x0, y0), n10 = hashf(x1, y0);
    float n01 = hashf(x0, y1), n11 = hashf(x1, y1);
    float nx0 = n00 + (n10 - n00) * sx;
    float nx1 = n01 + (n11 - n01) * sx;
    return nx0 + (nx1 - nx0) * sy;
}

}  // namespace

float fbm(float x, float y, int octaves) {
    float value = 0, amplitude = 0.5f, freq = 1.0f;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * valueNoise(x * freq, y * freq);
        freq *= 2.0f;
        amplitude *= 0.5f;
    }
    return value;
}

// Inigo Quilez-style domain warp: sample position is displaced by two
// independent fbm fields before the final fbm sample, producing the
// organic, marbled/cloud-like quality rather than plain blobby noise.
float warpedNoise(float x, float y) {
    float qx = fbm(x + 0.0f, y + 0.0f, 3);
    float qy = fbm(x + 5.2f, y + 1.3f, 3);
    return fbm(x + 4.0f * qx, y + 4.0f * qy, 3);
}

}  // namespace domain_noise
