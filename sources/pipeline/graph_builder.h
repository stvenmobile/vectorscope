#pragma once
#include <string>
#include <vector>
#include "graph_types.h"

// Turns concept labels + their embedding vectors into a weighted graph
// (cosine similarity, pruned to the strongest edges per node). Knows
// nothing about how the graph will be laid out or drawn.
namespace graph_builder {

ConceptGraph build(const std::vector<std::string>& labels,
                    const std::vector<std::vector<float>>& vectors,
                    int topKEdgesPerNode,
                    float canvasW, float canvasH);

}  // namespace graph_builder
