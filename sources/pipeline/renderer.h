#pragma once
#include <string>
#include "graph_types.h"

// Draws the current graph state and a status strip using LVGL. Knows
// nothing about how the graph was built or what stage the pipeline is in -
// just draws what it's given. The graph itself is drawn onto an LVGL canvas
// (full-redraw each call), which rides on LVGL's own double-buffered,
// VSYNC-synced flush - the thing that actually fixed the ESP32-S3 RGB
// "screen drift" issue that the earlier Arduino/LovyanGFX attempt hit.
namespace renderer {

static const int STATUS_BAR_HEIGHT = 36;

// Call once at startup, after LVGL is initialized. width/height are the
// full panel dimensions (800x480 on this board).
void init(int panelWidth, int panelHeight);

void drawGraph(const ConceptGraph& graph);
void drawStatus(const std::string& status);

int graphAreaWidth();
int graphAreaHeight();

}  // namespace renderer
