#include "renderer.h"
#include <algorithm>
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "adapters/lvgl/lvgl_port_v8.h"

namespace renderer {

namespace {

static const char* TAG = "renderer";

int g_panelWidth = 0;
int g_panelHeight = 0;

lv_obj_t* g_canvas = nullptr;
void* g_canvasBuf = nullptr;
lv_obj_t* g_statusLabel = nullptr;

void drawCenteredText(const std::string& text, int cx, int cy, const lv_font_t* font,
                       lv_color_t color) {
    lv_point_t size;
    lv_txt_get_size(&size, text.c_str(), font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);

    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.color = color;
    dsc.font = font;

    lv_canvas_draw_text(g_canvas, cx - size.x / 2, cy - size.y / 2, size.x + 4, &dsc,
                         text.c_str());
}

void drawFilledCircle(int cx, int cy, int radius, lv_color_t color) {
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color = color;
    dsc.width = radius;
    lv_canvas_draw_arc(g_canvas, cx, cy, radius, 0, 360, &dsc);
}

}  // namespace

void init(int panelWidth, int panelHeight) {
    g_panelWidth = panelWidth;
    g_panelHeight = panelHeight;

    lvgl_port_lock(-1);

    int canvasH = graphAreaHeight();
    size_t bufSize = panelWidth * canvasH * sizeof(lv_color_t);
    g_canvasBuf = heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "canvas buffer: requested %u bytes, got %p", (unsigned)bufSize, g_canvasBuf);

    g_canvas = lv_canvas_create(lv_scr_act());
    ESP_LOGI(TAG, "canvas obj: %p", g_canvas);
    lv_canvas_set_buffer(g_canvas, g_canvasBuf, panelWidth, canvasH, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(g_canvas, 0, 0);
    lv_canvas_fill_bg(g_canvas, lv_color_black(), LV_OPA_COVER);
    ESP_LOGI(TAG, "canvas init done");

    g_statusLabel = lv_label_create(lv_scr_act());
    lv_obj_set_size(g_statusLabel, panelWidth, STATUS_BAR_HEIGHT);
    lv_obj_set_pos(g_statusLabel, 0, canvasH);
    lv_obj_set_style_bg_color(g_statusLabel, lv_palette_darken(LV_PALETTE_BLUE, 3), 0);
    lv_obj_set_style_bg_opa(g_statusLabel, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(g_statusLabel, lv_color_white(), 0);
    lv_obj_set_style_pad_left(g_statusLabel, 10, 0);
    lv_obj_set_style_pad_top(g_statusLabel, (STATUS_BAR_HEIGHT - 16) / 2, 0);
    lv_label_set_long_mode(g_statusLabel, LV_LABEL_LONG_CLIP);
    lv_label_set_text(g_statusLabel, "");

    lvgl_port_unlock();
}

void drawGraph(const ConceptGraph& graph) {
    lvgl_port_lock(-1);

    lv_canvas_fill_bg(g_canvas, lv_color_black(), LV_OPA_COVER);

    for (const GraphEdge& e : graph.edges) {
        const GraphNode& a = graph.nodes[e.a];
        const GraphNode& b = graph.nodes[e.b];
        uint8_t brightness = (uint8_t)std::min(255.0f, std::max(30.0f, e.weight * 255.0f));

        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = lv_color_make(brightness / 3, brightness, brightness);
        dsc.width = 1;

        lv_point_t points[2] = {{(lv_coord_t)a.x, (lv_coord_t)a.y},
                                 {(lv_coord_t)b.x, (lv_coord_t)b.y}};
        lv_canvas_draw_line(g_canvas, points, 2, &dsc);
    }

    for (const GraphNode& node : graph.nodes) {
        drawFilledCircle((int)node.x, (int)node.y, 5, lv_color_make(255, 165, 0));
        drawCenteredText(node.label, (int)node.x, (int)node.y - 14, &lv_font_montserrat_16,
                          lv_color_white());
    }

    lv_obj_invalidate(g_canvas);
    lvgl_port_unlock();
}

void drawStatus(const std::string& status) {
    lvgl_port_lock(-1);
    lv_label_set_text(g_statusLabel, status.c_str());
    lvgl_port_unlock();
}

int graphAreaWidth() {
    return g_panelWidth;
}

int graphAreaHeight() {
    return g_panelHeight - STATUS_BAR_HEIGHT;
}

}  // namespace renderer
