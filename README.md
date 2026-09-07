# vectorscope

An ESP32-S3 generative-art display, running on the [Elecrow CrowPanel Advance 5.0"
HMI/AI display](https://www.elecrow.com/pub/wiki/CrowPanel_Advance_5.0-HMI_ESP32_AI_Display.html)
(ESP32-S3-WROOM-1-N16R8, 800x480 RGB IPS touchscreen, V1.1 board revision). Built on
ESP-IDF + LVGL, driving the RGB panel directly for full-screen animated visuals rather
than going through LVGL's widget/canvas pipeline.

The physical build uses Elecrow's own [3D-printed
case](https://www.elecrow.com/sharepj/crowpanel-50-inch-make-it-like-a-real-one-631.html?srsltid=AfmBOoqFyg-txuHIrt1r5ndB8Jw3Masx20zaqh0fl6Ix4mzEEyN2EPO3).

## Visuals

The display runs a small plugin system (see `sources/visuals/`) - each visual implements
a common `init`/`update`/`deinit`(/`onTouch`) interface and is listed in
`sources/visuals/registry.cpp`. The first entry in that list is the default visual on
boot. Tapping the **top-right corner** of the screen (within ~60px of the corner) always
cycles to the next visual, regardless of which one is active; touching anywhere else is
forwarded to the current visual instead.

- **Image Warp** *(default on boot)* - domain-warp distortion applied to a photo loaded
  from the SD card, cycling smoothly between a clear, undistorted view of the image and a
  fully rippled "liquid glass" version of it and back, over about 3 minutes. A subtle
  color drift fades in only while distorted, so the clear moments are always a pristine,
  unaltered view of the source photo. Touching anywhere except the corner hotspot loads
  the next image found on the card.
- **Noise Field** - a slowly-drifting, domain-warped noise field rendered as a soft color
  gradient. Fully procedural, no image required.
- **Kaleidoscope** - a touch-driven kaleidoscope using true triangular-mirror reflection
  (repeatedly reflecting each point across three mirror lines until it lands in the
  fundamental triangle), not just rotational repetition - so arbitrary, asymmetric
  "jewel" shapes are folded into a genuinely symmetric tiled pattern, the way a real
  3-mirror kaleidoscope tube works. Touching the screen (outside the corner hotspot)
  moves the mirror center to your finger.

Noise Field and Kaleidoscope need nothing beyond the firmware. Image Warp needs a
prepared SD card - see below.

## SD card setup (for Image Warp)

The card slot on this board shares its data lines with the onboard I2S microphone via an
analog switch, controlled by a small physical DIP switch silkscreened "Function Select"
near the RTC battery. **Both switch segments must be set to `1`** (the "MIC & TF Card"
row in the switch's own truth table) or the SD card will fail to initialize - this cost
significant debugging time before being traced back to the switch defaulting to `0,0`
from the factory. If `sdcard: Failed to init SD card` shows up in the serial log, check
this switch first.

Image requirements:
- **FAT32** filesystem (ESP-IDF's built-in FATFS driver doesn't support exFAT without a
  licensed component - reformat the card in Windows if it shipped exFAT).
- Images must be **uncompressed 24-bit BMP, exactly 800x480**. The loader
  (`sources/adapters/sdcard.cpp`) reads the classic 54-byte BMP header directly rather
  than using a general-purpose image library, so both compression and any size other than
  800x480 will be rejected.
- Short filenames (e.g. `IMAGE.BMP`) - ESP-IDF's FATFS defaults to 8.3 filenames unless
  long-filename support is explicitly enabled.

To prepare an image in GIMP: crop to a 5:3 aspect ratio (`Tools > Crop`, set "Fixed:
Aspect ratio" to `5:3` in Tool Options, drag to frame the shot, then **press Enter to
actually apply the crop** - it's easy to export before committing it, which silently
exports the uncompressed original instead), then `Image > Scale Image` to `800x480`,
`Image > Flatten Image`, then `File > Export As` a `.bmp` with both "Run-Length Encoded"
and "Write color space information" left **unchecked** (either one shifts the header away
from the classic 54-byte layout the loader expects).

Copy the prepared `.bmp` file(s) to the card's root directory. Image Warp picks up every
valid file it finds at boot; touching the screen cycles between them.

No images are included in this repository - see [Licensing note](#licensing-note) below.

## Architecture notes

- **Direct framebuffer rendering.** Visuals write straight into a PSRAM pixel buffer and
  push it to the panel with one `drawBitmap()` call per frame
  (`sources/adapters/display.cpp`), bypassing LVGL's canvas widget entirely. LVGL's
  configured draw buffer on this board is only 20 scanlines tall, so routing full-screen
  animation through it meant every frame was sliced into 24 separate composite+flush
  passes - capping animation well under 1fps. Direct presentation removed that ceiling.
- **Coarse-grid noise, upscaled.** All three visuals compute their domain-warp noise
  field on a coarse 40x24 grid and bilinearly (or, for Kaleidoscope's sharp mirror edges,
  block-wise) upscale it to full resolution, rather than evaluating the noise function
  per pixel. The shared noise primitives live in `sources/visuals/domain_noise.h/.cpp`.
- **`-O2`, not the default debug build.** This project's `sdkconfig.defaults` explicitly
  selects performance optimization; ESP-IDF (and Elecrow's own example projects) default
  to unoptimized debug builds, which was costing a large chunk of frame rate for no
  benefit here.

## Building

Standard ESP-IDF (v5.4) project.

```
idf.py build
idf.py -p <PORT> flash
```

## Licensing note

Photos used for testing Image Warp during development (a public-domain-style flower
photo, an ocean wave wallpaper, and a piece of copyrighted fine-art photography) are
deliberately **not** included in this repository. Image Warp loads whatever `.bmp` files
it finds on the SD card at runtime, so the code itself carries no licensing dependency on
any specific image - bring your own.
