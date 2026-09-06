#pragma once
#include <string>
#include <vector>
#include <functional>
#include "graph_types.h"

// Turns a list of concept strings into their embedding vectors. Knows
// nothing about graphs or layout - just words in, vectors out.
namespace embeddings {

// onProgress(i, total) is called after each vector is fetched, so the caller
// can update a status display without this module knowing about rendering.
bool fetchAll(const std::vector<std::string>& concepts,
              std::vector<std::vector<float>>& vectorsOut,
              std::function<void(int, int)> onProgress = nullptr);

}  // namespace embeddings
