#pragma once

#include <cstdint>
#include <string>

namespace npc {

struct ModelConfig {
    bool is_remote = false;
    std::string remote_provider;
    std::string remote_endpoint;
    std::string remote_api_key;
    std::string remote_model_name;

    std::string model_path;

    int32_t n_ctx          = 8192;
    int32_t n_batch        = 512;
    int32_t n_ubatch       = 512;
    int32_t n_threads      = 0;
    int32_t n_gpu_layers   = 999;
    bool    flash_attn     = true;
    bool    no_perf        = true;

    float   temperature    = 0.8f;
    float   temp_delta     = 0.0f;
    float   temp_exponent  = 1.0f;
    int32_t top_k          = 40;
    float   top_p          = 0.95f;
    float   min_p          = 0.05f;
    float   typical_p      = 1.0f;
    float   xtc_probability = 0.0f;
    float   xtc_threshold  = 0.10f;
    int32_t min_keep       = 1;

    int32_t penalty_last_n  = 64;
    float   penalty_repeat  = 1.10f;
    float   penalty_freq    = 0.0f;
    float   penalty_present = 0.0f;
    bool    penalize_nl     = false;

    int32_t n_predict     = 512;
    bool    ignore_eos    = false;
    int32_t seed          = -1;

    static ModelConfig defaults() { return ModelConfig{}; }
};

struct ModelMeta {
    std::string name;
    std::string path;
    std::string description;
    int64_t     param_count = 0;
    uint64_t    file_size   = 0;
    int32_t     n_ctx_train = 0;
    int32_t     n_embd      = 0;
    int32_t     n_layer     = 0;
    int32_t     n_vocab     = 0;
    bool        is_loaded   = false;
};

} // namespace npc
