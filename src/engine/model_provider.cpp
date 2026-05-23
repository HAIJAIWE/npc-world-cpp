#include "engine/model_provider.h"
#include "engine/model_engine.h"
#include "engine/api_model_backend.h"
#include <chrono>
#include <thread>

namespace npc {

ModelProvider& ModelProvider::instance() {
    static ModelProvider provider;
    return provider;
}

void ModelProvider::configure(const std::string& modelId, const ModelProviderConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    ModelProviderConfig cfg = config;
    cfg.model_id = modelId;
    m_configs[modelId] = cfg;

    const char* typeStr = cfg.type == ProviderType::REMOTE ? "REMOTE" : "LOCAL";
    fprintf(stdout, "[ModelProvider] %s -> %s (type=%s, fallback=%s)\n",
        modelId.c_str(), cfg.type == ProviderType::REMOTE ? cfg.remote_provider.c_str() : cfg.local_path.c_str(),
        typeStr, cfg.fallback_to_local ? "yes" : "no");
}

void ModelProvider::unconfigure(const std::string& modelId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_configs.erase(modelId);
}

const ModelProviderConfig* ModelProvider::getConfig(const std::string& modelId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_configs.find(modelId);
    return it != m_configs.end() ? &it->second : nullptr;
}

bool ModelProvider::isRemote(const std::string& modelId) const {
    auto* cfg = getConfig(modelId);
    return cfg && cfg->type == ProviderType::REMOTE;
}

bool ModelProvider::isLocal(const std::string& modelId) const {
    auto* cfg = getConfig(modelId);
    return cfg && cfg->type == ProviderType::LOCAL;
}

std::vector<std::string> ModelProvider::modelIds() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> ids;
    ids.reserve(m_configs.size());
    for (const auto& [id, _] : m_configs) ids.push_back(id);
    return ids;
}

ProviderResult ModelProvider::generateLocal(const std::string& modelId,
                                              const std::string& prompt,
                                              const ModelProviderConfig& cfg,
                                              ProviderTokenCallback tokenCb,
                                              const StopCriteria* stopCriteria) {
    ProviderResult result;
    result.source = ProviderType::LOCAL;

    auto& engine = ModelEngine::instance();

    if (!engine.isModelLoaded()) {
        result.error = "Local model not loaded";
        return result;
    }

    auto start = std::chrono::steady_clock::now();

    ModelEngine::TokenCallback engineCb = nullptr;
    if (tokenCb) {
        engineCb = [&tokenCb](const std::string& text, bool done) -> bool {
            return tokenCb(text, done);
        };
    }

    auto genResult = engine.generate(prompt, &cfg.params, engineCb, stopCriteria);

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    result.text = genResult.text;
    result.prompt_tokens = genResult.n_prompt_tokens;
    result.completion_tokens = genResult.n_tokens;
    result.tokens_per_second = genResult.tokens_per_second;
    result.success = !genResult.text.empty();

    if (!result.success) result.error = "Local generation produced empty output";

    m_stats.local_calls++;
    return result;
}

ProviderResult ModelProvider::generateRemote(const std::string& modelId,
                                               const std::vector<std::pair<std::string, std::string>>& messages,
                                               const std::string& systemPrompt,
                                               const ModelProviderConfig& cfg,
                                               ProviderTokenCallback tokenCb) {
    ProviderResult result;
    result.source = ProviderType::REMOTE;

    auto& backend = ApiModelBackend::instance();
    if (!backend.providerReady(cfg.remote_provider)) {
        result.error = "Remote provider not ready: " + cfg.remote_provider;
        m_stats.errors++;
        return result;
    }

    ApiStreamCallback streamCb = nullptr;
    if (tokenCb) {
        streamCb = [&tokenCb](const std::string& text, bool done) -> bool {
            return tokenCb(text, done);
        };
    }

    auto apiResult = backend.chat(cfg.remote_provider, messages, systemPrompt);

    result.text = apiResult.text;
    result.success = apiResult.success;
    result.error = apiResult.error;
    result.prompt_tokens = apiResult.prompt_tokens;
    result.completion_tokens = apiResult.completion_tokens;
    result.elapsed_ms = (double)apiResult.elapsed_ms;
    result.tokens_per_second = apiResult.tokens_per_second;

    if (result.success) {
        m_stats.remote_calls++;
    } else {
        m_stats.errors++;
    }

    if (!result.success && cfg.fallback_to_local) {
        fprintf(stdout, "[ModelProvider] %s remote failed, falling back to local\n", modelId.c_str());
        m_stats.fallback_calls++;

        std::string fullPrompt;
        for (const auto& [role, content] : messages) {
            fullPrompt += role + ": " + content + "\n";
        }

        result = generateLocal(modelId, fullPrompt, cfg, tokenCb, nullptr);
    }

    return result;
}

ProviderResult ModelProvider::generate(
    const std::string& modelId,
    const std::string& prompt,
    ProviderTokenCallback tokenCb,
    const StopCriteria* stopCriteria) {

    auto* cfg = getConfig(modelId);
    if (!cfg) {
        ProviderResult r;
        r.error = "Model not configured: " + modelId;
        return r;
    }

    if (cfg->type == ProviderType::LOCAL) {
        return generateLocal(modelId, prompt, *cfg, tokenCb, stopCriteria);
    }

    std::vector<std::pair<std::string, std::string>> messages = {
        {"user", prompt}
    };
    return generateRemote(modelId, messages, "", *cfg, tokenCb);
}

ProviderResult ModelProvider::generateMessages(
    const std::string& modelId,
    const std::vector<std::pair<std::string, std::string>>& messages,
    const std::string& systemPrompt,
    ProviderTokenCallback tokenCb) {

    auto* cfg = getConfig(modelId);
    if (!cfg) {
        ProviderResult r;
        r.error = "Model not configured: " + modelId;
        return r;
    }

    if (cfg->type == ProviderType::LOCAL) {
        auto& engine = ModelEngine::instance();
        if (!engine.isModelLoaded()) {
            ProviderResult r;
            r.error = "Local model not loaded";
            return r;
        }

        auto start = std::chrono::steady_clock::now();
        npc::ModelEngine::TokenCallback localTkCb = nullptr;
        if (tokenCb) {
            localTkCb = [&](const std::string& t, bool d) -> bool { return tokenCb(t, d); };
        }
        auto text = engine.completionMessages(messages, &cfg->params, localTkCb);
        auto end = std::chrono::steady_clock::now();

        ProviderResult result;
        result.source = ProviderType::LOCAL;
        result.text = text;
        result.success = !text.empty();
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        m_stats.local_calls++;
        return result;
    }

    return generateRemote(modelId, messages, systemPrompt, *cfg, tokenCb);
}

void ModelProvider::generateAsync(
    const std::string& modelId,
    const std::string& prompt,
    ProviderTokenCallback tokenCb,
    const StopCriteria* stopCriteria,
    ProviderCompletionCallback onComplete) {

    std::thread worker([this, modelId, prompt, tokenCb, stopCriteria, onComplete]() {
        auto result = generate(modelId, prompt, tokenCb, stopCriteria);
        if (onComplete) onComplete(result);
    });
    worker.detach();
}

} // namespace npc
