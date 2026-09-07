# vectorscope

An ESP32-S3 (CrowPanel Advance 5.0", 800x480 RGB LCD) generative-art display. Built on
ESP-IDF + LVGL, driving the RGB panel directly for full-screen animated visuals.

## Visuals

The display runs a small plugin system (see `sources/visuals/`) - each visual implements
a common `init`/`update`/`deinit`(/`onTouch`) interface and is listed in
`sources/visuals/registry.cpp`. Tapping the top-right corner of the screen cycles to the
next visual.

- **Noise Field** - a slowly-drifting, domain-warped noise field rendered as a soft color
  gradient.
- **Kaleidoscope** - a touch-driven kaleidoscope using true triangular-mirror reflection
  (not just rotational repetition), so arbitrary "jewel" shapes are folded into a
  symmetric tiled pattern. Touch moves the mirror center to your finger.
- **Image warp** *(planned)* - domain-warp distortion applied to a source photo instead
  of procedural noise, cycling between a clear, undistorted view of the image and a
  fully rippled "liquid glass" version of it, drifting back and forth between the two
  over the course of a few minutes.

  The source image is loaded from an SD card at runtime rather than being embedded in
  firmware, so this visual works with whatever image you supply - pick anything you find
  interesting to watch slowly warp and unwarp. No image is included in this repository.

## Building

Standard ESP-IDF (v5.4) project.

```
idf.py build
idf.py -p <PORT> flash
```
