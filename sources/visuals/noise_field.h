#pragma once

// A slowly-drifting, domain-warped noise field rendered as a soft color
// gradient - think ink diffusing in water or the aurora, not distinct
// shapes. Deliberately low-contrast and continuous. Self-contained: no
// network, no input, just a full-screen ambient visual.
namespace noise_field {

void init(int width, int height);

// Call periodically (e.g. every ~150-300ms). Advances the field's internal
// clock by dtSeconds and redraws.
void update(float dtSeconds);

// Tears down the canvas and frees its buffer. Call before another visual
// takes over the screen.
void deinit();

}  // namespace noise_field
