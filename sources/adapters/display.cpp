#include "display.h"

namespace display {

namespace {
esp_panel::drivers::LCD* g_lcd = nullptr;
}

void init(esp_panel::drivers::LCD* lcd) {
    g_lcd = lcd;
}

void present(const uint16_t* pixels, int width, int height) {
    if (!g_lcd) {
        return;
    }
    g_lcd->drawBitmap(0, 0, width, height, reinterpret_cast<const uint8_t*>(pixels));
}

}  // namespace display
