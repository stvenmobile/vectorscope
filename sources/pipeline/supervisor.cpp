#include "supervisor.h"
#include "config.h"
#include "secrets.h"
#include "entities/WIFI.h"
#include "ollama_client.h"
#include "concepts.h"
#include "embeddings.h"
#include "graph_builder.h"
#include "layout.h"
#include "renderer.h"
#include "esp_timer.h"

namespace supervisor {

namespace {

ConceptGraph g_graph;
Stage g_stage = Stage::IDLE;
bool g_queryRequested = false;
int64_t g_lastRedrawUs = 0;

// Now that LVGL's own double-buffered, VSYNC-synced flush (see renderer.cpp
// and lvgl_port_v8.cpp) properly solves the ESP32-S3 RGB "screen drift"
// issue, WiFi no longer needs to be toggled on/off around each query as a
// workaround the way the earlier Arduino attempt required - it stays
// connected throughout.
const int64_t REDRAW_INTERVAL_US = 300000;  // 300ms

void setStatus(const std::string& s) {
    renderer::drawStatus(s);
}

void runPipeline() {
    g_stage = Stage::SENDING_CHAT;
    setStatus(std::string("Asking: ") + MVP_PROMPT);

    std::string response;
    if (!ollama::chat(CHAT_MODEL, MVP_PROMPT, response)) {
        setStatus("Chat request failed - tap to retry");
        g_stage = Stage::ERROR;
        return;
    }

    g_stage = Stage::EXTRACTING_CONCEPTS;
    setStatus("Extracting concepts...");
    std::vector<std::string> conceptList;
    if (!concepts::extract(response, conceptList, MAX_CONCEPTS)) {
        setStatus("Concept extraction failed - tap to retry");
        g_stage = Stage::ERROR;
        return;
    }

    g_stage = Stage::FETCHING_EMBEDDINGS;
    std::vector<std::vector<float>> vectors;
    bool ok = embeddings::fetchAll(conceptList, vectors, [](int i, int total) {
        setStatus("Fetching embeddings (" + std::to_string(i) + "/" + std::to_string(total) + ")");
    });
    if (!ok) {
        setStatus("Embedding fetch failed - tap to retry");
        g_stage = Stage::ERROR;
        return;
    }

    g_stage = Stage::BUILDING_GRAPH;
    setStatus("Building graph...");
    g_graph = graph_builder::build(conceptList, vectors, TOP_K_EDGES_PER_NODE,
                                    renderer::graphAreaWidth(), renderer::graphAreaHeight());

    g_stage = Stage::LAYOUT_RUNNING;
    g_lastRedrawUs = esp_timer_get_time();
    setStatus(std::string("Live: ") + MVP_PROMPT + " (tap to re-ask)");
}

}  // namespace

void begin() {
    g_stage = Stage::IDLE;
    setStatus("Connecting to WiFi...");

    WIFI::instance().init();
    // connectAP unconditionally memcpy's from bssid (WIFI.cpp:126) even
    // though the actual connection is SSID-based, not BSSID-locked - a
    // nullptr here causes a LoadProhibited crash, so pass a real (if
    // unused) buffer instead.
    uint8_t zeroBssid[6] = {0};
    bool connected = WIFI::instance().connectAP(WIFI_SSID, zeroBssid, WIFI_PASSWORD, true, true);
    setStatus(connected ? "Ready - tap to start" : "WiFi connect failed - tap to retry");
}

void update() {
    if (g_queryRequested) {
        g_queryRequested = false;
        runPipeline();
    }
    if (g_stage == Stage::LAYOUT_RUNNING) {
        int64_t now = esp_timer_get_time();
        if (now - g_lastRedrawUs >= REDRAW_INTERVAL_US) {
            layout::step(g_graph, renderer::graphAreaWidth(), renderer::graphAreaHeight(),
                         (now - g_lastRedrawUs) / 1000000.0f);
            renderer::drawGraph(g_graph);
            g_lastRedrawUs = now;
        }
    }
}

void requestNewQuery() {
    g_queryRequested = true;
}

Stage currentStage() {
    return g_stage;
}

}  // namespace supervisor
