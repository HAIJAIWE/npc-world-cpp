#pragma once

#include "model_config.h"

#include <cstdint>
#include <string>
#include <vector>

namespace npc {

enum class MessageRole : uint8_t {
    System    = 0,
    User      = 1,
    Assistant = 2,
    NPC       = 3
};

struct ChatMessage {
    int64_t     id        = 0;
    std::string npc_id;
    std::string player_name;
    MessageRole speaker    = MessageRole::User;
    std::string content;
    int64_t     timestamp  = 0;
};

struct GenerateRequest {
    uint32_t             request_id;
    std::string          npc_id;
    std::string          npc_name;
    std::string          player_name;
    std::string          user_input;
    std::string          system_prompt;
    ModelConfig          params;
    std::vector<ChatMessage> history;
};

struct GenerateResult {
    uint32_t    request_id;
    bool        success;
    std::string full_text;
    std::string error_msg;
    int32_t     total_tokens;
    double      t_elapsed_ms;
    double      tokens_per_sec;
    bool        stopped_by_eos;
};

struct TokenEntry {
    uint32_t    request_id;
    std::string text;
    uint32_t    token_index;
};

} // namespace npc
