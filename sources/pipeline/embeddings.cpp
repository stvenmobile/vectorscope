#include "embeddings.h"
#include "esp_log.h"
#include "ollama_client.h"
#include "config.h"

namespace embeddings {

namespace {
static const char* TAG = "embeddings";
}

bool fetchAll(const std::vector<std::string>& concepts,
              std::vector<std::vector<float>>& vectorsOut,
              std::function<void(int, int)> onProgress) {
    vectorsOut.clear();
    vectorsOut.reserve(concepts.size());

    for (size_t i = 0; i < concepts.size(); i++) {
        std::vector<float> vec(EMBEDDING_DIM);
        if (!ollama::embed(EMBED_MODEL, concepts[i], vec.data(), EMBEDDING_DIM)) {
            ESP_LOGE(TAG, "fetchAll: failed on \"%s\"", concepts[i].c_str());
            return false;
        }
        vectorsOut.push_back(std::move(vec));
        if (onProgress) {
            onProgress((int)i + 1, (int)concepts.size());
        }
    }
    return true;
}

}  // namespace embeddings
