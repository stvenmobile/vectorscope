#pragma once

// Common interface every visual plugs into. main.cpp only ever talks to
// visuals through this - it has no idea noise_field or any other visual
// exists, so adding a new one is just: write init/update(/deinit), add one
// line to registry.cpp.
namespace visuals {

struct Visual {
    const char* name;
    void (*init)(int width, int height);
    void (*update)(float dtSeconds);
    // Optional - a visual only needs this if it allocates something
    // (canvas buffer, LVGL objects) that must be torn down before another
    // visual takes over the screen. Null if there's nothing to clean up.
    void (*deinit)();
    // Optional - continuous touch input (screen coordinates), for visuals
    // that react to where/whether the screen is being touched, not just
    // the tap-to-cycle gesture main.cpp handles itself. Called every loop
    // iteration while a touch is down (and once more on release), never
    // for a touch that started in the corner cycle-visual hotspot. Null if
    // the visual doesn't care about touch.
    void (*onTouch)(int x, int y, bool pressed);
};

}  // namespace visuals
