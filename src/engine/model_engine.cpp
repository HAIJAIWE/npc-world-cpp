#include "engine/model_engine.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <thread>

namespace npc {

ModelEngine& ModelEngine::instance() {
    static ModelEngine engine;
    return engine;
}

ModelEngine::~ModelEngine() {
    shutdown();
}

bool ModelEngine::initialize() {
    if (m_initialized) return true;

    llama_backend_init();

    m_initialized = true;
    return true;
}

void ModelEngine::shutdown() {
    if (m_generating.load()) {
        fprintf(stderr, "[ModelEngine] Cannot shutdown while generating\n");
        return;
    }

    unloadModel();
    llama_backend_free();
    m_initialized = false;
}

static int detectGpuLayers(int requested) {
    if (requested <= 0) return 0;
    int maxDevices = static_cast<int>(llama_max_devices());
    if (maxDevices <= 0) {
        fprintf(stdout, "[ModelEngine] No GPU detected, falling back to CPU\n");
        return 0;
    }
    fprintf(stdout, "[ModelEngine] %d GPU device(s) detected, offloading %d layers\n",
        maxDevices, requested);
    return requested;
}

bool ModelEngine::loadModel(const std::string& modelPath, ProgressCallback progress) {
    if (!m_initialized) {
        fprintf(stderr, "[ModelEngine] Not initialized\n");
        return false;
    }

    unloadModel();

    if (progress) progress(0.0f, "Loading model...");

    auto mparams = llama_model_default_params();
    mparams.n_gpu_layers = detectGpuLayers(m_defaultParams.n_gpu_layers);

    m_model = llama_model_load_from_file(modelPath.c_str(), mparams);
    if (!m_model) {
        fprintf(stderr, "[ModelEngine] Failed to load model: %s\n", modelPath.c_str());
        return false;
    }

    if (progress) progress(0.5f, "Creating context...");

    auto cparams = llama_context_default_params();
    cparams.n_ctx = m_defaultParams.n_ctx;
    cparams.n_batch = m_defaultParams.n_batch;
    cparams.n_ubatch = m_defaultParams.n_ubatch;
    cparams.n_threads = m_defaultParams.n_threads > 0 ? m_defaultParams.n_threads :
        std::max(1u, std::thread::hardware_concurrency());
    cparams.n_threads_batch = cparams.n_threads;
    cparams.flash_attn_type = m_defaultParams.flash_attn
        ? LLAMA_FLASH_ATTN_TYPE_ENABLED
        : LLAMA_FLASH_ATTN_TYPE_DISABLED;
    cparams.no_perf = m_defaultParams.no_perf;
    cparams.type_k = GGML_TYPE_Q4_0;
    cparams.type_v = GGML_TYPE_Q4_0;

    m_ctx = llama_init_from_model(m_model, cparams);
    if (!m_ctx) {
        fprintf(stderr, "[ModelEngine] Failed to create context\n");
        llama_model_free(m_model);
        m_model = nullptr;
        return false;
    }

    m_vocab = llama_model_get_vocab(m_model);

    m_modelInfo.name = modelPath.substr(modelPath.find_last_of("/\\") + 1);
    m_modelInfo.path = modelPath;

    char descBuf[1024];
    int32_t descLen = llama_model_desc(m_model, descBuf, sizeof(descBuf));
    m_modelInfo.description = descLen > 0 ? std::string(descBuf, descLen) : "";

    m_modelInfo.param_count = llama_model_n_params(m_model);
    m_modelInfo.file_size = llama_model_size(m_model);
    m_modelInfo.n_ctx_train = llama_model_n_ctx_train(m_model);
    m_modelInfo.n_embd = llama_model_n_embd(m_model);
    m_modelInfo.n_layer = llama_model_n_layer(m_model);
    m_modelInfo.n_vocab = llama_vocab_n_tokens(m_vocab);
    m_modelInfo.is_loaded = true;

    if (m_defaultParams.n_threads > 0) {
        setThreadCount(m_defaultParams.n_threads, m_defaultParams.n_threads);
    }

    if (progress) progress(1.0f, "Model loaded");

    fprintf(stdout, "[ModelEngine] Model loaded: %s (%lld params, %d ctx)\n",
        m_modelInfo.name.c_str(), (long long)m_modelInfo.param_count, m_defaultParams.n_ctx);

    return true;
}

void ModelEngine::unloadModel() {
    if (m_ctx) {
        llama_free(m_ctx);
        m_ctx = nullptr;
    }
    if (m_model) {
        llama_model_free(m_model);
        m_model = nullptr;
    }
    m_vocab = nullptr;
    m_modelInfo = ModelInfo{};
    m_modelInfo.is_loaded = false;
    freeSampler(m_cachedSampler);
    m_cachedSampler = nullptr;
    m_samplerCached = false;
    delete[] m_tokenDataBuf;
    m_tokenDataBuf = nullptr;
    m_tokenDataBufSize = 0;
}

struct llama_sampler* ModelEngine::getOrCreateSampler(const ModelParams* params) {
    const ModelParams& p = params ? *params : m_defaultParams;

    std::lock_guard<std::mutex> lock(m_samplerMutex);
    if (m_samplerCached && m_cachedSampler && p == m_cachedSamplerParams) {
        llama_sampler_reset(m_cachedSampler);
        return m_cachedSampler;
    }

    freeSampler(m_cachedSampler);
    m_cachedSampler = nullptr;
    m_samplerCached = false;

    auto sparams = llama_sampler_chain_default_params();
    sparams.no_perf = true;
    auto chain = llama_sampler_chain_init(sparams);

    if (p.penalty_last_n > 0 && (p.penalty_repeat > 1.0f || p.penalty_freq > 0.0f || p.penalty_present > 0.0f)) {
        auto penalties = llama_sampler_init_penalties(
            p.penalty_last_n,
            p.penalty_repeat,
            p.penalty_freq,
            p.penalty_present
        );
        llama_sampler_chain_add(chain, penalties);
    }

    if (p.top_k > 0) {
        llama_sampler_chain_add(chain, llama_sampler_init_top_k(p.top_k));
    }

    if (p.top_p < 1.0f) {
        llama_sampler_chain_add(chain, llama_sampler_init_top_p(p.top_p, p.min_keep));
    }

    if (p.min_p > 0.0f && p.min_p < 1.0f) {
        llama_sampler_chain_add(chain, llama_sampler_init_min_p(p.min_p, p.min_keep));
    }

    if (p.typical_p < 1.0f) {
        llama_sampler_chain_add(chain, llama_sampler_init_typical(p.typical_p, p.min_keep));
    }

    if (p.temp_exponent > 0.0f && p.temp_exponent != 1.0f) {
        llama_sampler_chain_add(chain, llama_sampler_init_temp_ext(p.temperature, p.temp_delta, p.temp_exponent));
    } else {
        llama_sampler_chain_add(chain, llama_sampler_init_temp(p.temperature));
    }

    if (p.xtc_probability > 0.0f) {
        static uint32_t xtc_seed = 42;
        llama_sampler_chain_add(chain, llama_sampler_init_xtc(p.xtc_probability, p.xtc_threshold, p.min_keep, xtc_seed++));
    }

    uint32_t distSeed = (p.seed >= 0) ? static_cast<uint32_t>(p.seed) : static_cast<uint32_t>(llama_time_us());
    llama_sampler_chain_add(chain, llama_sampler_init_dist(distSeed));

    m_cachedSampler = chain;
    m_cachedSamplerParams = p;
    m_samplerCached = true;

    return chain;
}

void ModelEngine::ensureTokenDataBuffer(int32_t nVocab) {
    if (m_tokenDataBuf && m_tokenDataBufSize >= nVocab) {
        return;
    }
    delete[] m_tokenDataBuf;
    m_tokenDataBuf = new llama_token_data[nVocab];
    m_tokenDataBufSize = nVocab;
}

void ModelEngine::freeSampler(struct llama_sampler* sampler) {
    if (sampler) {
        llama_sampler_free(sampler);
    }
}

GenerationResult ModelEngine::generate(
    const std::string& prompt,
    const ModelParams* params,
    TokenCallback tokenCallback,
    const StopCriteria* stopCriteria)
{
    GenerationResult result;
    if (!m_ctx || !m_model) return result;

    m_generating.store(true);

    const ModelParams& p = params ? *params : m_defaultParams;
    int32_t nPredict = p.n_predict > 0 ? p.n_predict : 512;

    auto tokens = tokenize(prompt, true);
    if (tokens.empty()) {
        m_generating.store(false);
        return result;
    }

    result.n_prompt_tokens = static_cast<int32_t>(tokens.size());

    int64_t tStart = llama_time_us();
    int64_t tPromptEnd = tStart;

    auto memory = llama_get_memory(m_ctx);
    llama_pos nPast = llama_memory_seq_pos_max(memory, 0);
    if (nPast < 0) nPast = 0;

    for (size_t i = 0; i < tokens.size(); i += m_defaultParams.n_batch) {
        size_t batchSize = std::min(tokens.size() - i, static_cast<size_t>(m_defaultParams.n_batch));
        auto batch = llama_batch_get_one(&tokens[i], static_cast<int32_t>(batchSize));
        batch.n_tokens = static_cast<int32_t>(batchSize);
        for (int j = 0; j < batch.n_tokens; j++) {
            batch.pos[j] = nPast + static_cast<llama_pos>(i) + j;
            batch.logits[j] = (i + j == tokens.size() - 1);
            batch.n_seq_id[j] = 1;
            static llama_seq_id seqId = 0;
            batch.seq_id[j][0] = seqId;
        }
        if (llama_decode(m_ctx, batch) != 0) {
            fprintf(stderr, "[ModelEngine] Decode failed during prompt eval\n");
            m_generating.store(false);
            return result;
        }
    }

    tPromptEnd = llama_time_us();
    result.t_prompt_ms = (tPromptEnd - tStart) / 1000.0;

    auto chain = getOrCreateSampler(params);
    std::string fullText;
    nPast += static_cast<llama_pos>(tokens.size());
    int32_t nVocab = m_modelInfo.n_vocab;
    ensureTokenDataBuffer(nVocab);

    for (int i = 0; i < nPredict; i++) {
        llama_token_data_array cur_p;
        cur_p.data = m_tokenDataBuf;
        cur_p.size = 0;
        cur_p.selected = -1;
        cur_p.sorted = false;

        float* logits = llama_get_logits_ith(m_ctx, 0);

        cur_p.size = nVocab;
        for (int j = 0; j < nVocab; j++) {
            cur_p.data[j] = { static_cast<llama_token>(j), logits[j], 0.0f };
        }

        llama_sampler_apply(chain, &cur_p);
        llama_sampler_reset(chain);

        llama_token newToken = cur_p.data[0].id;

        TokenInfo tinfo;
        tinfo.id = newToken;
        tinfo.text = detokenizeSingle(newToken);
        tinfo.logit = cur_p.data[0].logit;
        tinfo.prob = cur_p.data[0].p;
        result.tokens.push_back(tinfo);

        bool isEos = (newToken == eosToken()) || (newToken == llama_vocab_eot(m_vocab));
        if (isEos) {
            result.stopped_by_eos = true;
            if (!p.ignore_eos) {
                llama_sampler_accept(chain, newToken);
                break;
            }
        }

        fullText += tinfo.text;

        if (checkStopCriteria(fullText, stopCriteria, isEos)) {
            llama_sampler_accept(chain, newToken);
            break;
        }

        if (tokenCallback) {
            bool cont = tokenCallback(fullText, false);
            if (!cont) {
                llama_sampler_accept(chain, newToken);
                break;
            }
        }

        llama_sampler_accept(chain, newToken);

        auto batch = llama_batch_get_one(&newToken, 1);
        batch.n_tokens = 1;
        batch.pos[0] = nPast;
        batch.logits[0] = true;
        batch.n_seq_id[0] = 1;
        batch.seq_id[0][0] = 0;

        if (llama_decode(m_ctx, batch) != 0) {
            fprintf(stderr, "[ModelEngine] Decode failed at token %d\n", i);
            break;
        }

        nPast++;
    }

    result.text = fullText;
    result.n_tokens = static_cast<int32_t>(result.tokens.size());

    int64_t tEnd = llama_time_us();
    result.t_gen_ms = (tEnd - tPromptEnd) / 1000.0;
    if (result.t_gen_ms > 0) {
        result.tokens_per_second = result.n_tokens / (result.t_gen_ms / 1000.0);
    }

    if (tokenCallback) {
        tokenCallback(fullText, true);
    }

    m_generating.store(false);
    return result;
}

void ModelEngine::generateAsync(
    const std::string& prompt,
    const ModelParams* params,
    TokenCallback tokenCallback,
    const StopCriteria* stopCriteria,
    CompletionCallback onComplete)
{
    std::string p = prompt;
    ModelParams cp = params ? *params : m_defaultParams;
    auto cb = std::move(tokenCallback);
    auto sc = stopCriteria ? std::make_shared<StopCriteria>(*stopCriteria) : nullptr;

    m_workers.emplace_back([this, p, cp, cb, sc, onComplete]() {
        GenerationResult result = generate(p, &cp, cb, sc.get());
        if (onComplete) {
            onComplete(result);
        }
    });
    m_workers.back().detach();
}

GenerationResult ModelEngine::generateWithHistory(
    const std::vector<std::string>& history,
    const std::string& prompt,
    TokenCallback tokenCallback,
    const StopCriteria* stopCriteria)
{
    std::ostringstream fullPrompt;
    for (const auto& h : history) {
        fullPrompt << h << "\n";
    }
    fullPrompt << prompt;
    return generate(fullPrompt.str(), nullptr, tokenCallback, stopCriteria);
}

std::string ModelEngine::completion(const std::string& systemPrompt, const std::string& userPrompt,
                                    const ModelParams* params, TokenCallback tokenCallback)
{
    std::string prompt = buildChatPrompt(systemPrompt, userPrompt);
    auto result = generate(prompt, params, tokenCallback);
    return result.text;
}

std::string ModelEngine::completionMessages(
    const std::vector<std::pair<std::string, std::string>>& messages,
    const ModelParams* params, TokenCallback tokenCallback)
{
    std::string prompt = buildChatMessages(messages);
    auto result = generate(prompt, params, tokenCallback);
    return result.text;
}

std::string ModelEngine::buildChatPrompt(const std::string& system, const std::string& user) {
    std::ostringstream oss;
    oss << "<|im_start|>system\n" << system << "<|im_end|>\n";
    oss << "<|im_start|>user\n" << user << "<|im_end|>\n";
    oss << "<|im_start|>assistant\n";
    return oss.str();
}

std::string ModelEngine::buildChatMessages(const std::vector<std::pair<std::string, std::string>>& messages) {
    std::ostringstream oss;
    for (const auto& [role, content] : messages) {
        oss << "<|im_start|>" << role << "\n" << content << "<|im_end|>\n";
    }
    oss << "<|im_start|>assistant\n";
    return oss.str();
}

bool ModelEngine::checkStopCriteria(const std::string& text, const StopCriteria* criteria, bool eos) {
    if (!criteria) return false;

    if (criteria->stop_on_eos && eos) return true;

    if (criteria->max_tokens > 0) return false;

    for (const auto& stopStr : criteria->stop_strings) {
        if (!stopStr.empty() && text.find(stopStr) != std::string::npos) {
            return true;
        }
    }

    return false;
}

std::vector<llama_token> ModelEngine::tokenize(const std::string& text, bool addSpecial) {
    std::vector<llama_token> result;
    if (!m_vocab) return result;

    int32_t n = llama_tokenize(m_vocab, text.c_str(), static_cast<int32_t>(text.size()),
                                nullptr, 0, addSpecial, true);
    if (n < 0) {
        n = -n;
        result.resize(n);
        llama_tokenize(m_vocab, text.c_str(), static_cast<int32_t>(text.size()),
                       result.data(), n, addSpecial, true);
    } else if (n > 0) {
        result.resize(n);
        llama_tokenize(m_vocab, text.c_str(), static_cast<int32_t>(text.size()),
                       result.data(), n, addSpecial, true);
    }

    return result;
}

std::string ModelEngine::detokenize(const std::vector<llama_token>& tokens) {
    std::string result;
    for (auto token : tokens) {
        result += detokenizeSingle(token);
    }
    return result;
}

std::string ModelEngine::detokenizeSingle(llama_token token) {
    if (!m_vocab) return "";

    char buf[256];
    int32_t n = llama_token_to_piece(m_vocab, token, buf, sizeof(buf), 0, true);
    if (n < 0) {
        n = -n;
        std::vector<char> bigBuf(n + 1);
        llama_token_to_piece(m_vocab, token, bigBuf.data(), n, 0, true);
        bigBuf[n] = 0;
        return std::string(bigBuf.data());
    }
    buf[n] = 0;
    return std::string(buf);
}

int32_t ModelEngine::getKVTokenCount() const {
    if (!m_ctx) return 0;
    auto memory = llama_get_memory(m_ctx);
    llama_pos pos = llama_memory_seq_pos_max(memory, 0);
    return pos >= 0 ? static_cast<int32_t>(pos + 1) : 0;
}

void ModelEngine::clearKVCache() {
    if (m_ctx) {
        llama_memory_clear(llama_get_memory(m_ctx), true);
    }
}

void ModelEngine::setKVCacheSequenceShift(llama_seq_id seqId, llama_pos delta) {
    if (m_ctx) llama_memory_seq_add(llama_get_memory(m_ctx), seqId, 0, -1, delta);
}

void ModelEngine::removeKVCacheSequence(llama_seq_id seqId, llama_pos p0, llama_pos p1) {
    if (m_ctx) llama_memory_seq_rm(llama_get_memory(m_ctx), seqId, p0, p1);
}

void ModelEngine::setDefaultParams(const ModelParams& params) {
    m_defaultParams = params;
}

ModelParams ModelEngine::defaultParams() const {
    return m_defaultParams;
}

llama_token ModelEngine::eosToken() const {
    return m_vocab ? llama_vocab_eos(m_vocab) : -1;
}

llama_token ModelEngine::bosToken() const {
    return m_vocab ? llama_vocab_bos(m_vocab) : -1;
}

int32_t ModelEngine::vocabSize() const {
    return m_vocab ? llama_vocab_n_tokens(m_vocab) : 0;
}

void ModelEngine::setThreadCount(int32_t nThreads, int32_t nThreadsBatch) {
    if (m_ctx) llama_set_n_threads(m_ctx, nThreads, nThreadsBatch);
}

} // namespace npc
