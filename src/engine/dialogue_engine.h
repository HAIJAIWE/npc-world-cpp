#pragma once

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <cstdint>
#include <memory>
#include "engine/llm_cache.h"

namespace npc {

enum class SpeakerSelectionMode {
    RoundRobin,
    EmotionDriven,
    EventTriggered,
    Manual
};

struct ConversationConfig {
    SpeakerSelectionMode speaker_mode = SpeakerSelectionMode::RoundRobin;
    int max_turns = 20;
    int max_tokens_per_turn = 256;
    float temperature = 0.8f;
    bool use_memory = true;
    bool use_world_state = true;
    bool use_cache = true;
    bool use_tools = false;
    int max_tool_rounds = 3;
    std::string system_prompt_template;
};

struct ConversationTurn {
    int turn_index;
    std::string speaker_id;
    std::string speaker_name;
    std::string listener_id;
    std::string content;
    std::string emotion;
    float emotion_intensity = 0.0f;
    int64_t timestamp_generated = 0;
};

struct ActiveConversation {
    std::string conversation_id;
    std::string topic;
    std::string location_id;
    std::vector<std::string> participant_ids;
    std::vector<std::string> participant_names;
    ConversationConfig config;
    std::deque<ConversationTurn> history;
    int current_speaker_index = 0;
    int turn_count = 0;
    int64_t started_at = 0;
    int64_t last_turn_at = 0;
    bool active = false;
};

class DialogueEngine {
public:
    using TurnCallback = std::function<void(const ActiveConversation&, const ConversationTurn&)>;

    static DialogueEngine& instance();

    void configure(const ConversationConfig& defaultCfg);

    std::string startConversation(const std::string& topic,
                                   const std::string& locationId,
                                   const std::vector<std::string>& participantIds,
                                   const std::vector<std::string>& participantNames,
                                   const ConversationConfig* cfg = nullptr);

    bool advanceConversation(const std::string& conversationId);

    bool endConversation(const std::string& conversationId);

    bool injectPlayerMessage(const std::string& conversationId,
                              const std::string& speakerId,
                              const std::string& speakerName,
                              const std::string& content);

    ActiveConversation* getConversation(const std::string& conversationId);
    const ActiveConversation* getConversation(const std::string& conversationId) const;

    std::vector<std::string> activeConversationIds() const;

    void setTurnCallback(TurnCallback cb);
    void setStopCondition(const std::string& conversationId, int maxTurns);

    std::string buildSystemPrompt(const ActiveConversation& conv) const;
    std::string buildUserPrompt(const ActiveConversation& conv,
                                 const std::string& speakerId,
                                 const std::string& speakerName,
                                 const std::string& listenerId) const;
    std::string buildFullPrompt(const ActiveConversation& conv,
                                 const std::string& speakerId,
                                 const std::string& speakerName,
                                 const std::string& listenerId) const;

    std::string selectNextSpeaker(const ActiveConversation& conv) const;
    float getSpeakerPriority(const ActiveConversation& conv,
                             const std::string& npcId) const;

private:
    DialogueEngine() = default;
    DialogueEngine(const DialogueEngine&) = delete;
    DialogueEngine& operator=(const DialogueEngine&) = delete;

    std::string generateConversationId();
    std::string nextSpeakerRoundRobin(const ActiveConversation& conv) const;
    std::string nextSpeakerEmotionDriven(const ActiveConversation& conv) const;
    std::string getNpcName(const std::string& npcId) const;
    uint32_t hashPromptText(const std::string& text) const;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, ActiveConversation> m_conversations;
    int64_t m_nextConvId = 1;
    ConversationConfig m_defaultConfig;
    TurnCallback m_turnCallback;

    LLMCache* m_cache = nullptr;
};

} // namespace npc
