#pragma once

// Applies the same domain-warp distortion noise_field uses, but to a photo
// loaded from the SD card instead of a color palette - so the image
// ripples like liquid glass. The warp strength breathes from perfectly
// clear up to full distortion and back over a few minutes, and a subtle
// color drift (tied to the same warp field) fades in only while distorted,
// so the "clear" moments are always a pristine, unaltered view of the
// source photo. Touching the screen loads the next image on the card.
namespace image_warp {

void init(int width, int height);
void update(float dtSeconds);
void deinit();
void onTouch(int x, int y, bool pressed);

}  // namespace image_warp
