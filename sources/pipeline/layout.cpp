#include "layout.h"
#include <algorithm>
#include <cmath>
#include "esp_random.h"

namespace {

const float REPULSION_STRENGTH = 12000.0f;
const float SPRING_STRENGTH = 0.06f;
const float IDEAL_EDGE_LENGTH = 90.0f;
const float DAMPING = 0.90f;
const float JITTER = 3.0f;      // keeps the layout gently alive at rest
const float MAX_SPEED = 200.0f;
const float MARGIN = 40.0f;

// esp_random() gives uint32_t; scale to a symmetric [-1, 1] float.
float randomUnit() {
    return (int32_t)(esp_random() % 2001 - 1000) / 1000.0f;
}

}  // namespace

namespace layout {

void step(ConceptGraph& graph, float canvasW, float canvasH, float dt) {
    int n = (int)graph.nodes.size();
    if (n == 0) return;

    std::vector<float> fx(n, 0), fy(n, 0);

    // Repulsion between every pair (n is small, ~20, so O(n^2) is cheap).
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            float dx = graph.nodes[i].x - graph.nodes[j].x;
            float dy = graph.nodes[i].y - graph.nodes[j].y;
            float distSq = dx * dx + dy * dy + 1.0f;
            float dist = sqrtf(distSq);
            float force = REPULSION_STRENGTH / distSq;
            float fxi = (dx / dist) * force;
            float fyi = (dy / dist) * force;
            fx[i] += fxi;
            fy[i] += fyi;
            fx[j] -= fxi;
            fy[j] -= fyi;
        }
    }

    // Spring attraction along edges - stronger similarity pulls harder.
    for (const GraphEdge& e : graph.edges) {
        float dx = graph.nodes[e.b].x - graph.nodes[e.a].x;
        float dy = graph.nodes[e.b].y - graph.nodes[e.a].y;
        float dist = sqrtf(dx * dx + dy * dy) + 0.001f;
        float displacement = dist - IDEAL_EDGE_LENGTH;
        float force = SPRING_STRENGTH * e.weight * displacement;
        float fxi = (dx / dist) * force;
        float fyi = (dy / dist) * force;
        fx[e.a] += fxi;
        fy[e.a] += fyi;
        fx[e.b] -= fxi;
        fy[e.b] -= fyi;
    }

    // Small continuous jitter so the layout never fully freezes.
    for (int i = 0; i < n; i++) {
        fx[i] += randomUnit() * JITTER;
        fy[i] += randomUnit() * JITTER;
    }

    for (int i = 0; i < n; i++) {
        GraphNode& node = graph.nodes[i];
        node.vx = (node.vx + fx[i] * dt) * DAMPING;
        node.vy = (node.vy + fy[i] * dt) * DAMPING;

        float speed = sqrtf(node.vx * node.vx + node.vy * node.vy);
        if (speed > MAX_SPEED) {
            node.vx = node.vx / speed * MAX_SPEED;
            node.vy = node.vy / speed * MAX_SPEED;
        }

        node.x += node.vx * dt;
        node.y += node.vy * dt;

        node.x = std::clamp(node.x, MARGIN, canvasW - MARGIN);
        node.y = std::clamp(node.y, MARGIN, canvasH - MARGIN);
    }
}

}  // namespace layout
