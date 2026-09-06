#include "concepts.h"
#include "cJSON.h"
#include "esp_log.h"
#include "ollama_client.h"
#include "config.h"

namespace concepts {

namespace {
static const char* TAG = "concepts";
}

bool extract(const std::string& llmResponse, std::vector<std::string>& conceptsOut,
             int maxConcepts) {
    std::string prompt =
        "Extract up to " + std::to_string(maxConcepts) +
        " key concept words or short phrases from the following text. "
        "Respond with ONLY a JSON object of the form "
        "{\"concepts\": [\"word1\", \"word2\", ...]} and nothing else.\n\n"
        "Text: " + llmResponse;

    std::string reply;
    if (!ollama::chat(CHAT_MODEL, prompt, reply, /*jsonMode=*/true)) {
        return false;
    }

    cJSON* doc = cJSON_Parse(reply.c_str());
    if (!doc) {
        ESP_LOGE(TAG, "JSON parse error near: %s", cJSON_GetErrorPtr());
        return false;
    }

    cJSON* arr = cJSON_GetObjectItem(doc, "concepts");
    if (!arr || !cJSON_IsArray(arr)) {
        ESP_LOGE(TAG, "no 'concepts' array in reply");
        cJSON_Delete(doc);
        return false;
    }

    conceptsOut.clear();
    cJSON* item;
    cJSON_ArrayForEach(item, arr) {
        if ((int)conceptsOut.size() >= maxConcepts) break;
        if (cJSON_IsString(item) && item->valuestring && item->valuestring[0] != '\0') {
            conceptsOut.push_back(item->valuestring);
        }
    }
    cJSON_Delete(doc);
    return !conceptsOut.empty();
}

}  // namespace concepts
