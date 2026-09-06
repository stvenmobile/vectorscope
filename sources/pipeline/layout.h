#pragma once
#include "graph_types.h"

// One step of a force-directed physics simulation on a ConceptGraph. Knows
// nothing about where the graph came from or how it will be drawn - just
// nudges positions/velocities each call. Meant to be called every frame,
// indefinitely (never "finishes"), so the layout stays gently alive rather
// than freezing once settled.
namespace layout {

void step(ConceptGraph& graph, float canvasW, float canvasH, float dt);

}  // namespace layout
