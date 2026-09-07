#include "noise_field.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "adapters/display.h"

namespace noise_field {

namespace {

int g_width = 0, g_height = 0;
uint16_t* g_canvasBuf = nullptr;
float g_time = 0;

// Noise is computed on a coarse grid and bilinearly upscaled to fill the
// screen - evaluating true per-pixel noise 800x480 times a frame would be
// wasteful. A grid cell every ~20px upscales smoothly enough that the
// domain warp's organic quality hides any blockiness.
const int GRID_W = 40;
const int GRID_H = 24;
std::vector<float> g_grid(GRID_W * GRID_H);

// Palette is precomputed into a lookup table once - the cosine-based
// palette formula needs 3 cosf() calls per sample, which is too expensive
// to run per-pixel (384,000 times) every frame. Indexing a 256-entry LUT
// per pixel is effectively free by comparison.
const int PALETTE_SIZE = 256;
uint16_t g_paletteLUT[PALETTE_SIZE];

// Bilinear interpolation from the coarse grid to each screen pixel only
// depends on screen/grid dimensions, which never change at runtime - so the
// (x0, x1, weight) lookup for every column and row is computed once here
// instead of being recomputed (with a division) for every one of the
// 384,000 pixels on every single frame. This is what makes a high frame
// rate possible: the per-pixel inner loop becomes pure array lookups and
// multiply-adds, no division/clamp/cast per pixel.
std::vector<int> g_colX0, g_colX1;
std::vector<float> g_colSx;
std::vector<int> g_rowY0, g_rowY1;
std::vector<float> g_rowSy;

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

void buildPaletteLUT() {
    // Cosine palette (Inigo Quilez's formula): color = a + b*cos(2pi*(c*t+d)).
    // Tuned toward muted blue/teal/purple - deliberately low-contrast/soft,
    // not saturated.
    const float a[3] = {0.35f, 0.40f, 0.50f};
    const float b[3] = {0.30f, 0.30f, 0.35f};
    const float c[3] = {1.0f, 0.9f, 0.6f};
    const float d[3] = {0.10f, 0.30f, 0.55f};

    for (int i = 0; i < PALETTE_SIZE; i++) {
        float t = (float)i / (PALETTE_SIZE - 1);
        float rf = a[0] + b[0] * cosf(6.2831853f * (c[0] * t + d[0]));
        float gf = a[1] + b[1] * cosf(6.2831853f * (c[1] * t + d[1]));
        float bf = a[2] + b[2] * cosf(6.2831853f * (c[2] * t + d[2]));
        uint8_t r = (uint8_t)(std::clamp(rf, 0.0f, 1.0f) * 255);
        uint8_t g = (uint8_t)(std::clamp(gf, 0.0f, 1.0f) * 255);
        uint8_t bch = (uint8_t)(std::clamp(bf, 0.0f, 1.0f) * 255);
        g_paletteLUT[i] = lv_color_make(r, g, bch).full;
    }
}

void computeGrid() {
    const float scale = 0.15f;   // spatial frequency of the base pattern
    const float driftX = 0.06f;  // field drift speed, grid-units/second
    const float driftY = 0.04f;
    for (int gy = 0; gy < GRID_H; gy++) {
        for (int gx = 0; gx < GRID_W; gx++) {
            float x = gx * scale + g_time * driftX;
            float y = gy * scale + g_time * driftY;
            g_grid[gy * GRID_W + gx] = warpedNoise(x, y);
        }
    }
}

void buildSampleTables(int width, int height) {
    g_colX0.resize(width);
    g_colX1.resize(width);
    g_colSx.resize(width);
    for (int px = 0; px < width; px++) {
        float gx = (float)px / (width - 1) * (GRID_W - 1);
        int x0 = (int)gx;
        g_colX0[px] = x0;
        g_colX1[px] = std::min(x0 + 1, GRID_W - 1);
        g_colSx[px] = gx - x0;
    }

    g_rowY0.resize(height);
    g_rowY1.resize(height);
    g_rowSy.resize(height);
    for (int py = 0; py < height; py++) {
        float gy = (float)py / (height - 1) * (GRID_H - 1);
        int y0 = (int)gy;
        g_rowY0[py] = y0;
        g_rowY1[py] = std::min(y0 + 1, GRID_H - 1);
        g_rowSy[py] = gy - y0;
    }
}

}  // namespace

void init(int width, int height) {
    g_width = width;
    g_height = height;

    buildPaletteLUT();
    buildSampleTables(width, height);

    size_t bufSize = (size_t)width * height * sizeof(uint16_t);
    g_canvasBuf = (uint16_t*)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);
}

void update(float dtSeconds) {
    g_time += dtSeconds;
    computeGrid();

    for (int py = 0; py < g_height; py++) {
        int y0 = g_rowY0[py], y1 = g_rowY1[py];
        float sy = g_rowSy[py];
        const float* rowA = &g_grid[y0 * GRID_W];
        const float* rowB = &g_grid[y1 * GRID_W];
        uint16_t* row = g_canvasBuf + py * g_width;

        for (int px = 0; px < g_width; px++) {
            int x0 = g_colX0[px], x1 = g_colX1[px];
            float sx = g_colSx[px];
            float nx0 = rowA[x0] + (rowA[x1] - rowA[x0]) * sx;
            float nx1 = rowB[x0] + (rowB[x1] - rowB[x0]) * sx;
            float n = nx0 + (nx1 - nx0) * sy;

            int idx = (int)(n * (PALETTE_SIZE - 1));
            if (idx < 0) idx = 0;
            else if (idx >= PALETTE_SIZE) idx = PALETTE_SIZE - 1;
            row[px] = g_paletteLUT[idx];
        }
    }

    display::present(g_canvasBuf, g_width, g_height);
}

void deinit() {
    if (g_canvasBuf) {
        heap_caps_free(g_canvasBuf);
        g_canvasBuf = nullptr;
    }

    g_time = 0;
}

}  // namespace noise_field
