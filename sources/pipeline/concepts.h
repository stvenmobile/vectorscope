#pragma once
#include <string>
#include <vector>

// Turns an LLM's free-text response into a bounded list of key concept
// words/phrases, suitable for embedding. Knows nothing about vectors or
// graphs - just text in, word list out.
namespace concepts {

bool extract(const std::string& llmResponse, std::vector<std::string>& conceptsOut,
             int maxConcepts = 18);

}  // namespace concepts
