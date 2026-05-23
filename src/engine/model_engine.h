#pragma once

#include "llama.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>

namespace npc {

struct ModelParams {
    float temperature      = 0.8f;
    float temp_delta       = 0.0f;
    float temp_exponent    = 1.0f;
    int32_t top_k          = 40;
    float top_p            = 0.95f;
    float min_p            = 0.05f;
    float typical_p        = 1.0f;
    float xtc_probability  = 0.0f;
    float xtc_threshold    = 0.10f;
    int32_t min_keep       = 1;
    int32_t penalty_last_n = 64;
    float penalty_repeat   = 1.10f;
    float penalty_freq     = 0.0f;
    float penalty_present  = 0.0f;
    bool penalize_nl       = false;
    bool ignore_eos        = false;
    int32_t n_predict      = 512;
    int32_t n_ctx          = 8192;
    int32_t n_batch        = 512;
    int32_t n_ubatch       = 512;
    int32_t n_threads      = 0;
    int32_t n_gpu_layers   = 999;
    bool flash_attn        = true;
    bool no_perf           = true;
    int32_t seed           = -1;

    bool operator==(const ModelParams& o) const {
        return temperature == o.temperature && temp_delta == o.temp_delta &&
               temp_exponent == o.temp_exponent && top_k == o.top_k &&
               top_p == o.top_p && min_p == o.min_p && typical_p == o.typical_p &&
               xtc_probability == o.xtc_probability && xtc_threshold == o.xtc_threshold &&
               min_keep == o.min_keep && penalty_last_n == o.penalty_last_n &&
               penalty_repeat == o.penalty_repeat && penalty_freq == o.penalty_freq &&
               penalty_present == o.penalty_present && penalize_nl == o.penalize_nl &&
               ignore_eos == o.ignore_eos;
    }
    bool operator!=(const ModelParams& o) const { return !(*this == o); }
};

struct ModelInfo {
    std::string name;
    std::string path;
    std::string description;
    int64_t param_count;
    uint64_t file_size;
    int32_t n_ctx_train;
    int32_t n_embd;
    int32_t n_layer;
    int32_t n_vocab;
    bool is_loaded;
};

struct TokenInfo {
    llama_token id;
    std::string text;
    float logit;
    float prob;
};

struct GenerationResult {
    std::string text;
    std::vector<TokenInfo> tokens;
    int32_t n_tokens;
    int32_t n_prompt_tokens;
    double t_gen_ms;
    double t_prompt_ms;
    double tokens_per_second;
    bool stopped_by_eos;
};

struct StopCriteria {
    std::vector<std::string> stop_strings;
    int32_t max_tokens = 0;
    bool stop_on_eos = true;
};

class ModelEngine {
public:
    using ProgressCallback = std::function<void(float progress, const std::string& status)>;
    using TokenCallback = std::function<bool(const std::string& text, bool done)>;
    using CompletionCallback = std::function<void(const GenerationResult&)>;

    static ModelEngine& instance();

    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    bool loadModel(const std::string& modelPath, ProgressCallback progress = nullptr);
    void unloadModel();
    bool isModelLoaded() const { return m_model != nullptr; }

    GenerationResult generate(
        const std::string& prompt,
        const ModelParams* params = nullptr,
        TokenCallback tokenCallback = nullptr,
        const StopCriteria* stopCriteria = nullptr);

    void generateAsync(
        const std::string& prompt,
        const ModelParams* params,
        TokenCallback tokenCallback,
        const StopCriteria* stopCriteria,
        CompletionCallback onComplete);

    GenerationResult generateWithHistory(
        const std::vector<std::string>& history,
        const std::string& prompt,
        TokenCallback tokenCallback = nullptr,
        const StopCriteria* stopCriteria = nullptr);

    std::string completion(const std::string& systemPrompt, const std::string& userPrompt,
                           const ModelParams* params = nullptr,
                           TokenCallback tokenCallback = nullptr);

    std::string completionMessages(
        const std::vector<std::pair<std::string, std::string>>& messages,
        const ModelParams* params = nullptr,
        TokenCallback tokenCallback = nullptr);

    std::vector<llama_token> tokenize(const std::string& text, bool addSpecial = true);
    std::string detokenize(const std::vector<llama_token>& tokens);
    std::string detokenizeSingle(llama_token token);

    int32_t getKVTokenCount() const;
    void clearKVCache();
    void setKVCacheSequenceShift(llama_seq_id seqId, llama_pos delta);
    void removeKVCacheSequence(llama_seq_id seqId, llama_pos p0, llama_pos p1);

    ModelInfo getModelInfo() const { return m_modelInfo; }
    ModelParams getDefaultParams() const { return m_defaultParams; }
    void setDefaultParams(const ModelParams& params);
    ModelParams defaultParams() const;

    bool isGenerating() const { return m_generating.load(); }

    llama_token eosToken() const;
    llama_token bosToken() const;
    int32_t vocabSize() const;

    void setThreadCount(int32_t nThreads, int32_t nThreadsBatch);

    struct llama_context* nativeContext() { return m_ctx; }
    const struct llama_model* nativeModel() { return m_model; }
    const struct llama_vocab* nativeVocab() { return m_vocab; }

private:
    ModelEngine() = default;
    ~ModelEngine();
    ModelEngine(const ModelEngine&) = delete;
    ModelEngine& operator=(const ModelEngine&) = delete;

    struct llama_sampler* getOrCreateSampler(const ModelParams* params = nullptr);
    void freeSampler(struct llama_sampler* sampler);
    std::string buildChatPrompt(const std::string& system, const std::string& user);
    std::string buildChatMessages(const std::vector<std::pair<std::string, std::string>>& messages);
    bool checkStopCriteria(const std::string& text, const StopCriteria* criteria, bool eos);

    void ensureTokenDataBuffer(int32_t nVocab);

    bool m_initialized = false;

    struct llama_model* m_model = nullptr;
    struct llama_context* m_ctx = nullptr;
    const struct llama_vocab* m_vocab = nullptr;

    ModelInfo m_modelInfo;
    ModelParams m_defaultParams;
    std::atomic<bool> m_generating{false};

    struct llama_sampler* m_cachedSampler = nullptr;
    ModelParams m_cachedSamplerParams;
    bool m_samplerCached = false;
    std::mutex m_samplerMutex;

    llama_token_data* m_tokenDataBuf = nullptr;
    int32_t m_tokenDataBufSize = 0;

    std::vector<std::thread> m_workers;
    std::atomic<bool> m_stopping{false};
};

} // namespace npc
