#include "ollama_client.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "adapters/HTTPRequest.h"
#include "secrets.h"

namespace {

static const char* TAG = "ollama_client";

// CPU-based inference can genuinely take well over a minute, especially the
// first request after Ollama (re)loads a model into memory.
const int TIMEOUT_MS = 180000;

std::string buildUrl(const char* path) {
    char buf[128];
    snprintf(buf, sizeof(buf), "http://%s:%u%s", OLLAMA_HOST, OLLAMA_PORT, path);
    return std::string(buf);
}

// Response buffers live in PSRAM - the embeddings response in particular
// (768 floats as JSON text) can run to tens of KB.
struct ScopedBuffer {
    char* ptr;
    explicit ScopedBuffer(size_t size) {
        ptr = (char*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }
    ~ScopedBuffer() {
        if (ptr) heap_caps_free(ptr);
    }
};

}  // namespace

namespace ollama {

bool chat(const std::string& model, const std::string& userPrompt, std::string& responseOut,
          bool jsonMode) {
    cJSON* reqDoc = cJSON_CreateObject();
    cJSON_AddStringToObject(reqDoc, "model", model.c_str());
    cJSON_AddBoolToObject(reqDoc, "stream", false);
    if (jsonMode) {
        cJSON_AddStringToObject(reqDoc, "format", "json");
    }
    cJSON* messages = cJSON_AddArrayToObject(reqDoc, "messages");
    cJSON* msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", userPrompt.c_str());
    cJSON_AddItemToArray(messages, msg);

    char* body = cJSON_PrintUnformatted(reqDoc);
    cJSON_Delete(reqDoc);

    const size_t bufSize = 8192;
    ScopedBuffer buf(bufSize);

    std::string url = buildUrl("/api/chat");
    HTTPRequest request = HTTPRequest::Builder()
                              .setUrl(url.c_str())
                              .setMethod(HTTP_METHOD_POST)
                              .setBuffer(buf.ptr, bufSize)
                              .setBody((const uint8_t*)body, strlen(body), "application/json")
                              .setTimeoutMs(TIMEOUT_MS)
                              .build();

    int received = request.perform();
    cJSON_free(body);

    if (received <= 0) {
        ESP_LOGE(TAG, "chat request failed");
        return false;
    }

    cJSON* respDoc = cJSON_ParseWithLength(buf.ptr, received);
    if (!respDoc) {
        ESP_LOGE(TAG, "chat response JSON parse error near: %s", cJSON_GetErrorPtr());
        return false;
    }

    cJSON* message = cJSON_GetObjectItem(respDoc, "message");
    cJSON* content = message ? cJSON_GetObjectItem(message, "content") : nullptr;
    if (!content || !cJSON_IsString(content)) {
        ESP_LOGE(TAG, "chat: no message.content in response");
        cJSON_Delete(respDoc);
        return false;
    }
    responseOut = content->valuestring;
    cJSON_Delete(respDoc);
    return true;
}

bool embed(const std::string& model, const std::string& text, float* outVec, int dim) {
    cJSON* reqDoc = cJSON_CreateObject();
    cJSON_AddStringToObject(reqDoc, "model", model.c_str());
    cJSON_AddStringToObject(reqDoc, "prompt", text.c_str());

    char* body = cJSON_PrintUnformatted(reqDoc);
    cJSON_Delete(reqDoc);

    const size_t bufSize = 32768;
    ScopedBuffer buf(bufSize);

    std::string url = buildUrl("/api/embeddings");
    HTTPRequest request = HTTPRequest::Builder()
                              .setUrl(url.c_str())
                              .setMethod(HTTP_METHOD_POST)
                              .setBuffer(buf.ptr, bufSize)
                              .setBody((const uint8_t*)body, strlen(body), "application/json")
                              .setTimeoutMs(TIMEOUT_MS)
                              .build();

    int received = request.perform();
    cJSON_free(body);

    if (received <= 0) {
        ESP_LOGE(TAG, "embed request failed for \"%s\"", text.c_str());
        return false;
    }

    cJSON* respDoc = cJSON_ParseWithLength(buf.ptr, received);
    if (!respDoc) {
        ESP_LOGE(TAG, "embed response JSON parse error near: %s", cJSON_GetErrorPtr());
        return false;
    }

    cJSON* vec = cJSON_GetObjectItem(respDoc, "embedding");
    if (!vec || !cJSON_IsArray(vec)) {
        ESP_LOGE(TAG, "embed: no embedding array in response");
        cJSON_Delete(respDoc);
        return false;
    }

    int n = std::min(cJSON_GetArraySize(vec), dim);
    for (int i = 0; i < n; i++) {
        outVec[i] = (float)cJSON_GetArrayItem(vec, i)->valuedouble;
    }
    for (int i = n; i < dim; i++) {
        outVec[i] = 0.0f;
    }
    cJSON_Delete(respDoc);
    return true;
}

}  // namespace ollama
