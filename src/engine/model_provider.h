#pragma once

#include "engine/model_engine.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <mutex>

namespace npc {

struct ModelParams;
struct GenerationResult;
struct StopCriteria;

enum class ProviderType : uint8_t {
    LOCAL,
    REMOTE
};

struct ModelProviderConfig {
    std::string model_id;
    ProviderType type = ProviderType::LOCAL;
    std::string local_path;
    std::string remote_provider;
    ModelParams params;
    bool fallback_to_local = false;
    bool enabled = true;
};

using ProviderTokenCallback = std::function<bool(const std::string& text, bool done)>;
using ProviderCompletionCallback = std::function<void(const struct ProviderResult&)>;

struct ProviderResult {
    bool success = false;
    std::string text;
    std::string error;
    int32_t prompt_tokens = 0;
    int32_t completion_tokens = 0;
    double elapsed_ms = 0.0;
    double tokens_per_second = 0.0;
    ProviderType source = ProviderType::LOCAL;
};

class ModelProvider {
public:
    static ModelProvider& instance();

    void configure(const std::string& modelId, const ModelProviderConfig& config);
    void unconfigure(const std::string& modelId);

    const ModelProviderConfig* getConfig(const std::string& modelId) const;

    ProviderResult generate(
        const std::string& modelId,
        const std::string& prompt,
        ProviderTokenCallback tokenCb = nullptr,
        const StopCriteria* stopCriteria = nullptr);

    ProviderResult generateMessages(
        const std::string& modelId,
        const std::vector<std::pair<std::string, std::string>>& messages,
        const std::string& systemPrompt = "",
        ProviderTokenCallback tokenCb = nullptr);

    void generateAsync(
        const std::string& modelId,
        const std::string& prompt,
        ProviderTokenCallback tokenCb,
        const StopCriteria* stopCriteria,
        ProviderCompletionCallback onComplete);

    bool isRemote(const std::string& modelId) const;
    bool isLocal(const std::string& modelId) const;

    std::vector<std::string> modelIds() const;

    struct Stats {
        int64_t local_calls = 0;
        int64_t remote_calls = 0;
        int64_t fallback_calls = 0;
        int64_t errors = 0;
    };
    Stats stats() const { return m_stats; }

private:
    ModelProvider() = default;
    ModelProvider(const ModelProvider&) = delete;
    ModelProvider& operator=(const ModelProvider&) = delete;

    ProviderResult generateLocal(const std::string& modelId, const std::string& prompt,
                                  const ModelProviderConfig& cfg,
                                  ProviderTokenCallback tokenCb,
                                  const StopCriteria* stopCriteria);
    ProviderResult generateRemote(const std::string& modelId,
                                   const std::vector<std::pair<std::string, std::string>>& messages,
                                   const std::string& systemPrompt,
                                   const ModelProviderConfig& cfg,
                                   ProviderTokenCallback tokenCb);

    std::unordered_map<std::string, ModelProviderConfig> m_configs;
    Stats m_stats;
    mutable std::mutex m_mutex;
};

} // namespace npc
