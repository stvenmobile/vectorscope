#pragma once

#include <cstdint>
#include "esp_display_panel.hpp"

// Raw full-frame presentation, bypassing LVGL entirely. LVGL's own draw
// buffer on this board is only 20 scanlines tall (no full-frame double
// buffer is configured), so pushing a full 800x480 frame through an LVGL
// canvas means 24 separate composite+flush passes with their own overhead
// each - that's what was capping full-screen animation at ~5fps. A visual
// that owns the whole screen and has no LVGL widgets can instead write its
// finished RGB565 frame directly into the panel's framebuffer in one shot.
namespace display {

void init(esp_panel::drivers::LCD* lcd);

// pixels must be a width*height RGB565 (uint16_t) buffer, top-left origin,
// row-major - the same layout produced by lv_canvas / heap_caps_malloc'd
// buffers used elsewhere in this project.
void present(const uint16_t* pixels, int width, int height);

}  // namespace display
