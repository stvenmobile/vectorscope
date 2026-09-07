#include "image_warp.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "adapters/display.h"
#include "adapters/sdcard.h"
#include "domain_noise.h"

namespace image_warp {

namespace {

const char* TAG = "image_warp";

int g_width = 0, g_height = 0;
uint16_t* g_sourceImage = nullptr;  // the loaded photo, RGB565
uint16_t* g_outputBuf = nullptr;    // warped+tinted frame actually presented

std::vector<std::string> g_imageNames;
int g_currentImage = 0;

float g_time = 0;

// Same coarse-grid-plus-bilinear-upscale approach as noise_field, for the
// same reason: evaluating the warp noise per-pixel (384,000 times/frame)
// would be far too slow.
const int GRID_W = 40;
const int GRID_H = 24;
std::vector<float> g_dispXGrid(GRID_W * GRID_H);
std::vector<float> g_dispYGrid(GRID_W * GRID_H);
std::vector<float> g_tintGrid(GRID_W * GRID_H);

std::vector<int> g_colX0, g_colX1;
std::vector<float> g_colSx;
std::vector<int> g_rowY0, g_rowY1;
std::vector<float> g_rowSy;

// The tint drift is deliberately low-amplitude (small b[] values) - this
// is meant to read as a subtle color shift while distorted, not a palette.
const int TINT_LUT_SIZE = 256;
uint16_t g_tintLUT[TINT_LUT_SIZE];

void buildTintLUT() {
    const float a[3] = {0.5f, 0.5f, 0.5f};
    const float b[3] = {0.10f, 0.08f, 0.12f};
    const float c[3] = {1.0f, 0.8f, 0.6f};
    const float d[3] = {0.0f, 0.2f, 0.5f};
    for (int i = 0; i < TINT_LUT_SIZE; i++) {
        float t = (float)i / (TINT_LUT_SIZE - 1);
        float rf = a[0] + b[0] * cosf(6.2831853f * (c[0] * t + d[0]));
        float gf = a[1] + b[1] * cosf(6.2831853f * (c[1] * t + d[1]));
        float bf = a[2] + b[2] * cosf(6.2831853f * (c[2] * t + d[2]));
        uint8_t r = (uint8_t)(std::clamp(rf, 0.0f, 1.0f) * 255);
        uint8_t g = (uint8_t)(std::clamp(gf, 0.0f, 1.0f) * 255);
        uint8_t bch = (uint8_t)(std::clamp(bf, 0.0f, 1.0f) * 255);
        g_tintLUT[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (bch >> 3);
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

float sampleBilinear(const std::vector<float>& grid, int px, int py) {
    int x0 = g_colX0[px], x1 = g_colX1[px];
    float sx = g_colSx[px];
    int y0 = g_rowY0[py], y1 = g_rowY1[py];
    float sy = g_rowSy[py];
    const float* rowA = &grid[y0 * GRID_W];
    const float* rowB = &grid[y1 * GRID_W];
    float nx0 = rowA[x0] + (rowA[x1] - rowA[x0]) * sx;
    float nx1 = rowB[x0] + (rowB[x1] - rowB[x0]) * sx;
    return nx0 + (nx1 - nx0) * sy;
}

void computeGrids() {
    const float scale = 0.15f;
    const float driftX = 0.05f, driftY = 0.03f;
    for (int gy = 0; gy < GRID_H; gy++) {
        for (int gx = 0; gx < GRID_W; gx++) {
            float x = gx * scale + g_time * driftX;
            float y = gy * scale + g_time * driftY;
            // Three offset samples of the same warp field give quasi-
            // independent displacement-x, displacement-y and tint values
            // without needing three unrelated noise functions.
            g_dispXGrid[gy * GRID_W + gx] = domain_noise::warpedNoise(x, y);
            g_dispYGrid[gy * GRID_W + gx] = domain_noise::warpedNoise(x + 37.1f, y + 91.7f);
            g_tintGrid[gy * GRID_W + gx] = domain_noise::warpedNoise(x + 12.3f, y + 58.4f);
        }
    }
}

void loadImage(int index) {
    if (g_imageNames.empty() || !g_sourceImage) return;
    int count = (int)g_imageNames.size();
    index = ((index % count) + count) % count;
    if (sdcard::loadBmp(g_imageNames[index], g_sourceImage, g_width, g_height)) {
        g_currentImage = index;
        ESP_LOGI(TAG, "Loaded %s", g_imageNames[index].c_str());
    } else {
        ESP_LOGE(TAG, "Failed to load %s", g_imageNames[index].c_str());
    }
}

}  // namespace

void init(int width, int height) {
    g_width = width;
    g_height = height;
    g_time = 0;

    buildTintLUT();
    buildSampleTables(width, height);

    size_t bufSize = (size_t)width * height * sizeof(uint16_t);
    g_sourceImage = (uint16_t*)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);
    g_outputBuf = (uint16_t*)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);

    g_imageNames = sdcard::listValidBmp(width, height);
    if (g_imageNames.empty()) {
        ESP_LOGE(TAG, "No usable %dx%d BMP files found on the SD card", width, height);
    }
    loadImage(0);
}

void update(float dtSeconds) {
    g_time += dtSeconds;
    computeGrids();

    if (!g_sourceImage || !g_outputBuf) return;

    // Full clear -> full distortion -> full clear once per cycle.
    const float CYCLE_SECONDS = 180.0f;
    float phase = fmodf(g_time, CYCLE_SECONDS) / CYCLE_SECONDS;
    float warpAmount = 0.5f - 0.5f * cosf(6.2831853f * phase);  // smooth 0..1..0

    const float MAX_DISPLACEMENT_PX = 55.0f;
    const float MAX_TINT_BLEND = 0.18f;
    float blendAmount = warpAmount * MAX_TINT_BLEND;

    // The warp/tint fields are smooth and coarse (bilinearly upscaled from
    // a 40x24 grid), so they barely change over a handful of pixels -
    // sampling them fresh per pixel (384,000 times/frame) was the expensive
    // part and is what capped this at ~2fps, visible as one jump per
    // second rather than continuous motion. A locally-constant offset
    // across a small block is visually indistinguishable from the
    // original per-pixel version, since the field was already smooth at
    // that scale. What must stay per-pixel is the actual photo lookup, so
    // full image detail is preserved - and at zero distortion the offset
    // is exactly 0 for every block, so the "clear" state is still
    // pixel-perfect regardless of block size.
    const int BLOCK = 4;

    for (int by = 0; by < g_height; by += BLOCK) {
        int blockH = std::min(BLOCK, g_height - by);
        for (int bx = 0; bx < g_width; bx += BLOCK) {
            int blockW = std::min(BLOCK, g_width - bx);

            float dxN = sampleBilinear(g_dispXGrid, bx, by) - 0.5f;
            float dyN = sampleBilinear(g_dispYGrid, bx, by) - 0.5f;
            int offsetX = (int)(dxN * 2.0f * MAX_DISPLACEMENT_PX * warpAmount);
            int offsetY = (int)(dyN * 2.0f * MAX_DISPLACEMENT_PX * warpAmount);

            uint16_t tintColor = 0;
            if (blendAmount > 0.001f) {
                float tintN = sampleBilinear(g_tintGrid, bx, by);
                int idx = std::clamp((int)(tintN * (TINT_LUT_SIZE - 1)), 0, TINT_LUT_SIZE - 1);
                tintColor = g_tintLUT[idx];
            }

            for (int yy = 0; yy < blockH; yy++) {
                int py = by + yy;
                int sy = std::clamp(py + offsetY, 0, g_height - 1);
                uint16_t* outRow = g_outputBuf + py * g_width;
                const uint16_t* srcRow = g_sourceImage + sy * g_width;

                for (int xx = 0; xx < blockW; xx++) {
                    int px = bx + xx;
                    int sx = std::clamp(px + offsetX, 0, g_width - 1);
                    uint16_t srcColor = srcRow[sx];

                    if (blendAmount > 0.001f) {
                        int sr = (srcColor >> 11) & 0x1F, sg = (srcColor >> 5) & 0x3F, sb = srcColor & 0x1F;
                        int tr = (tintColor >> 11) & 0x1F, tg = (tintColor >> 5) & 0x3F, tb = tintColor & 0x1F;
                        int rr = sr + (int)((tr - sr) * blendAmount);
                        int rg = sg + (int)((tg - sg) * blendAmount);
                        int rb = sb + (int)((tb - sb) * blendAmount);
                        outRow[px] = (uint16_t)((rr << 11) | (rg << 5) | rb);
                    } else {
                        outRow[px] = srcColor;
                    }
                }
            }
        }
    }

    display::present(g_outputBuf, g_width, g_height);
}

void deinit() {
    if (g_sourceImage) {
        heap_caps_free(g_sourceImage);
        g_sourceImage = nullptr;
    }
    if (g_outputBuf) {
        heap_caps_free(g_outputBuf);
        g_outputBuf = nullptr;
    }
    g_imageNames.clear();
}

void onTouch(int x, int y, bool pressed) {
    if (pressed) {
        loadImage(g_currentImage + 1);
    }
}

}  // namespace image_warp
