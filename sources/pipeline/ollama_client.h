#pragma once
#include <string>

// Low-level HTTP/JSON plumbing for talking to Ollama's chat and embeddings
// endpoints. Knows nothing about concepts or graphs - just "send text, get
// text back" / "send text, get a vector back".
namespace ollama {

// Blocking. Sends a single-turn chat request and returns the model's reply
// text. Returns false (and leaves responseOut untouched) on any network or
// parse failure. Pass jsonMode=true to force the model to reply with strict
// JSON (Ollama's "format":"json" mode) - use this when the caller needs to
// parse the reply as structured data rather than display it as prose.
bool chat(const std::string& model, const std::string& userPrompt, std::string& responseOut,
          bool jsonMode = false);

// Blocking. Fetches the embedding vector for `text`. `outVec` must have room
// for at least `dim` floats. Returns false on failure.
bool embed(const std::string& model, const std::string& text, float* outVec, int dim);

}  // namespace ollama
