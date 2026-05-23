#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <mutex>

namespace npc {

struct ApiModelConfig {
    std::string provider;
    std::string api_url;
    std::string api_key;
    std::string model_name;
    int max_tokens = 512;
    float temperature = 0.8f;
    float top_p = 0.95f;
    int timeout_ms = 30000;

    bool valid() const {
        return !provider.empty() && !api_url.empty() && !api_key.empty() && !model_name.empty();
    }
};

struct ApiModelResult {
    bool success = false;
    std::string text;
    std::string error;
    int status_code = 0;
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int64_t elapsed_ms = 0;
    double tokens_per_second = 0.0;
};

struct ApiModelStats {
    int64_t total_requests = 0;
    int64_t success_count = 0;
    int64_t error_count = 0;
    int64_t total_prompt_tokens = 0;
    int64_t total_completion_tokens = 0;
    int64_t total_elapsed_ms = 0;
    std::string last_error;
};

using ApiStreamCallback = std::function<bool(const std::string& text, bool done)>;

class ApiModelBackend {
public:
    static ApiModelBackend& instance();

    void addProvider(const std::string& name, const ApiModelConfig& config);
    bool removeProvider(const std::string& name);
    const ApiModelConfig* getProvider(const std::string& name) const;

    std::vector<std::string> providerNames() const;

    ApiModelResult chat(const std::string& provider,
                         const std::vector<std::pair<std::string, std::string>>& messages,
                         const std::string& systemPrompt = "");

    ApiModelResult chatStream(const std::string& provider,
                               const std::vector<std::pair<std::string, std::string>>& messages,
                               ApiStreamCallback callback,
                               const std::string& systemPrompt = "");

    bool providerReady(const std::string& name) const;

    ApiModelStats stats(const std::string& provider = "") const;
    ApiModelStats totalStats() const;

private:
    ApiModelBackend() = default;
    ApiModelBackend(const ApiModelBackend&) = delete;
    ApiModelBackend& operator=(const ApiModelBackend&) = delete;

    std::string buildOpenAiBody(const std::vector<std::pair<std::string, std::string>>& messages,
                                 const ApiModelConfig& config,
                                 const std::string& systemPrompt) const;
    std::string parseOpenAiResponse(const std::string& body,
                                     ApiModelResult& result) const;
    std::string parseStreamChunk(const std::string& chunk) const;

    std::unordered_map<std::string, ApiModelConfig> m_providers;
    std::unordered_map<std::string, ApiModelStats> m_stats;
    mutable std::mutex m_mutex;
};

} // namespace npc
