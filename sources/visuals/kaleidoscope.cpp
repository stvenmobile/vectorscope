#include "kaleidoscope.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "adapters/display.h"

// A real 3-mirror kaleidoscope tube has an equilateral-triangle cross
// section (each pair of mirror strips meets its neighbor at a 120 degree
// angle). Content placed anywhere in front of it gets reflected back and
// forth between the three mirrors, tiling the whole view with copies of
// whatever is inside the triangle - so the source content itself can be
// completely lopsided and asymmetric; the mirrors are what force the
// symmetry, not the content. This is implemented the standard way such
// kaleidoscope shaders/fractal folds do it: reflect a point across
// whichever mirror line it's on the wrong side of, repeat until it settles
// inside the triangle, then sample the (arbitrary, unmirrored) jewel
// layout at that folded position.
namespace kaleidoscope {

namespace {

int g_width = 0, g_height = 0;
uint16_t* g_buf = nullptr;

float g_centerX = 0, g_centerY = 0;
float g_rotation = 0;  // ambient rotation offset, radians

// Three mirror lines, all at the same perpendicular distance from the
// triangle's center (TRIANGLE_D), with outward normals 120 degrees apart -
// any such trio forms a valid equilateral triangle, regardless of which
// absolute angle we start from.
const float TRIANGLE_D = 150.0f;
struct { float nx, ny; } const kMirrors[3] = {
    {0.0f, 1.0f},
    {-0.8660254f, -0.5f},
    {0.8660254f, -0.5f},
};
const int MAX_FOLDS = 10;

struct Jewel {
    float cx, cy;  // arbitrary position - deliberately not symmetric
    float radius;
    float highlightDx, highlightDy;
    float highlightRadius;
    uint16_t colorMain;
    uint16_t colorHighlight;
};

std::vector<Jewel>* g_jewelsPtr = nullptr;

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return lv_color_make(r, g, b).full;
}

void buildJewels() {
    // Positions/sizes are irregular on purpose - the fold is what creates
    // the symmetric kaleidoscope image, not the source layout. Kept well
    // within the triangle's incircle (radius TRIANGLE_D) so they read
    // cleanly regardless of the ambient rotation.
    struct Def {
        float x, y, size;
        uint8_t r, g, b;
    };
    const Def defs[] = {
        {35.0f, 55.0f, 30.0f, 200, 30, 40},     // ruby
        {-70.0f, 15.0f, 20.0f, 30, 170, 90},    // emerald
        {15.0f, -85.0f, 24.0f, 40, 90, 210},    // sapphire
        {-35.0f, -45.0f, 16.0f, 150, 60, 200},  // amethyst
        {75.0f, -20.0f, 19.0f, 210, 170, 30},   // topaz
        {-15.0f, 5.0f, 12.0f, 40, 190, 190},    // aquamarine
    };

    g_jewelsPtr->clear();
    for (const Def& d : defs) {
        Jewel j;
        j.cx = d.x;
        j.cy = d.y;
        j.radius = d.size;
        j.highlightDx = -d.size * 0.3f;
        j.highlightDy = -d.size * 0.3f;
        j.highlightRadius = d.size * 0.35f;
        j.colorMain = rgb(d.r, d.g, d.b);
        j.colorHighlight = rgb(
            (uint8_t)std::min(255, d.r + 90), (uint8_t)std::min(255, d.g + 90), (uint8_t)std::min(255, d.b + 90));
        g_jewelsPtr->push_back(j);
    }
}

uint16_t shadeAt(float fx, float fy) {
    for (const Jewel& j : *g_jewelsPtr) {
        float dx = fx - j.cx, dy = fy - j.cy;
        float d2 = dx * dx + dy * dy;
        if (d2 <= j.radius * j.radius) {
            float hdx = fx - (j.cx + j.highlightDx);
            float hdy = fy - (j.cy + j.highlightDy);
            if (hdx * hdx + hdy * hdy <= j.highlightRadius * j.highlightRadius) {
                return j.colorHighlight;
            }
            return j.colorMain;
        }
    }
    return 0;  // black background
}

}  // namespace

void init(int width, int height) {
    g_width = width;
    g_height = height;
    g_centerX = width * 0.5f;
    g_centerY = height * 0.5f;
    g_rotation = 0;

    g_jewelsPtr = new std::vector<Jewel>();
    buildJewels();

    size_t bufSize = (size_t)width * height * sizeof(uint16_t);
    g_buf = (uint16_t*)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);
}

void update(float dtSeconds) {
    const float rotSpeed = 0.1f;  // radians/second, ambient drift when idle
    g_rotation += dtSeconds * rotSpeed;

    // The whole frame shares one rotation, so compute sin/cos once here
    // instead of once per pixel - rotating the query point is equivalent
    // to (and much cheaper than) rotating all three mirror lines.
    float cosR = cosf(g_rotation), sinR = sinf(g_rotation);

    // The fold-loop-plus-jewel-test math is heavy enough per pixel that
    // running it all 384,000 times a frame drops well under 1fps - visible
    // as the same "ticking clock" jump noise-field had before its own
    // coarse-grid fix. Same remedy here: compute the fold once per BLOCK
    // and paint the whole block that color, cutting the expensive math by
    // BLOCK*BLOCK. Unlike noise-field's bilinear upscale, this uses flat
    // nearest-neighbor blocks - blurring across a mirror seam would look
    // wrong, where a bit of blockiness at facet edges reads fine.
    const int BLOCK = 4;

    for (int by = 0; by < g_height; by += BLOCK) {
        int blockH = std::min(BLOCK, g_height - by);
        float dy0 = by - g_centerY;
        for (int bx = 0; bx < g_width; bx += BLOCK) {
            int blockW = std::min(BLOCK, g_width - bx);
            float dx0 = bx - g_centerX;

            float x = dx0 * cosR + dy0 * sinR;
            float y = -dx0 * sinR + dy0 * cosR;

            for (int iter = 0; iter < MAX_FOLDS; iter++) {
                bool reflected = false;
                for (int m = 0; m < 3; m++) {
                    float dist = x * kMirrors[m].nx + y * kMirrors[m].ny - TRIANGLE_D;
                    if (dist > 0.0f) {
                        x -= 2.0f * dist * kMirrors[m].nx;
                        y -= 2.0f * dist * kMirrors[m].ny;
                        reflected = true;
                    }
                }
                if (!reflected) break;
            }

            uint16_t color = shadeAt(x, y);
            for (int yy = 0; yy < blockH; yy++) {
                uint16_t* row = g_buf + (by + yy) * g_width + bx;
                for (int xx = 0; xx < blockW; xx++) {
                    row[xx] = color;
                }
            }
        }
    }

    display::present(g_buf, g_width, g_height);
}

void deinit() {
    if (g_buf) {
        heap_caps_free(g_buf);
        g_buf = nullptr;
    }
    if (g_jewelsPtr) {
        delete g_jewelsPtr;
        g_jewelsPtr = nullptr;
    }
}

void onTouch(int x, int y, bool pressed) {
    if (pressed) {
        g_centerX = (float)x;
        g_centerY = (float)y;
    }
}

}  // namespace kaleidoscope
