#pragma once
#include "graph_types.h"

// Owns the pipeline state machine and the current graph. Everything else
// (ollama_client, concepts, embeddings, graph_builder, layout, renderer) is
// a stateless-ish service this calls in sequence - this is the only module
// that knows the overall shape of the pipeline.
namespace supervisor {

enum class Stage {
    IDLE,
    SENDING_CHAT,
    EXTRACTING_CONCEPTS,
    FETCHING_EMBEDDINGS,
    BUILDING_GRAPH,
    LAYOUT_RUNNING,
    ERROR
};

void begin();

// Call periodically (e.g. every ~50ms from app_main's loop). Runs the
// layout step + redraw when a graph is live, and kicks off the pipeline
// (blocking) when a query is pending.
void update();

// Sets a flag checked at the top of the next update() - safe to call from
// touch handling.
void requestNewQuery();

Stage currentStage();

}  // namespace supervisor
