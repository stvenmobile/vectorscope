#pragma once

// A touch-driven kaleidoscope: a handful of jewel-like shapes are mirrored
// into a radially-symmetric pattern around a center point. Touching the
// screen moves that center to the touch location; letting go leaves it
// there. A slow ambient rotation keeps it from looking frozen when nobody
// is touching it.
namespace kaleidoscope {

void init(int width, int height);
void update(float dtSeconds);
void deinit();

// x, y are screen coordinates; pressed is true while the finger is down.
void onTouch(int x, int y, bool pressed);

}  // namespace kaleidoscope
