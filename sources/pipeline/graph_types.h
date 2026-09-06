#pragma once
#include <string>
#include <vector>

static const int EMBEDDING_DIM = 768;  // nomic-embed-text output size

struct GraphNode {
    std::string label;
    float x, y;    // current position, in canvas pixel space
    float vx, vy;  // velocity, for the force-directed layout
};

struct GraphEdge {
    int a, b;      // indices into ConceptGraph::nodes
    float weight;  // cosine similarity, 0..1
};

struct ConceptGraph {
    std::vector<GraphNode> nodes;
    std::vector<GraphEdge> edges;
};
