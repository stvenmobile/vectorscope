#include "graph_builder.h"
#include <algorithm>
#include <cmath>
#include "esp_random.h"

namespace {

float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    float dot = 0, na = 0, nb = 0;
    for (size_t i = 0; i < a.size(); i++) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (na <= 0 || nb <= 0) return 0;
    return dot / (sqrtf(na) * sqrtf(nb));
}

int randomInt(int maxExclusive) {
    return (int)(esp_random() % (uint32_t)maxExclusive);
}

}  // namespace

namespace graph_builder {

ConceptGraph build(const std::vector<std::string>& labels,
                    const std::vector<std::vector<float>>& vectors,
                    int topKEdgesPerNode,
                    float canvasW, float canvasH) {
    ConceptGraph graph;
    int n = (int)labels.size();

    graph.nodes.reserve(n);
    for (int i = 0; i < n; i++) {
        GraphNode node;
        node.label = labels[i];
        node.x = (float)randomInt((int)canvasW);
        node.y = (float)randomInt((int)canvasH);
        node.vx = 0;
        node.vy = 0;
        graph.nodes.push_back(node);
    }

    // Full pairwise similarity matrix (n is small, ~20, so O(n^2) is fine).
    std::vector<std::vector<float>> sim(n, std::vector<float>(n, 0));
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            float s = cosineSimilarity(vectors[i], vectors[j]);
            sim[i][j] = s;
            sim[j][i] = s;
        }
    }

    // Keep each node's strongest K neighbors, deduplicating (i,j) vs (j,i).
    std::vector<std::vector<bool>> kept(n, std::vector<bool>(n, false));
    for (int i = 0; i < n; i++) {
        std::vector<int> others;
        for (int j = 0; j < n; j++) {
            if (j != i) others.push_back(j);
        }
        std::sort(others.begin(), others.end(), [&](int a, int b) {
            return sim[i][a] > sim[i][b];
        });
        int k = std::min((int)others.size(), topKEdgesPerNode);
        for (int idx = 0; idx < k; idx++) {
            int j = others[idx];
            kept[i][j] = true;
            kept[j][i] = true;
        }
    }

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (kept[i][j]) {
                graph.edges.push_back({i, j, sim[i][j]});
            }
        }
    }

    return graph;
}

}  // namespace graph_builder
