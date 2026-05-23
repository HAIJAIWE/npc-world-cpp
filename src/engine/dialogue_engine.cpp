#include "engine/dialogue_engine.h"
#include "engine/model_engine.h"
#include "engine/model_registry.h"
#include "engine/api_client.h"
#include "engine/npc_memory.h"
#include "engine/llm_cache.h"
#include "engine/world_state.h"
#include "engine/context_manager.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <random>

namespace npc {

DialogueEngine& DialogueEngine::instance() {
    static DialogueEngine engine;
    return engine;
}

void DialogueEngine::configure(const ConversationConfig& defaultCfg) {
    m_defaultConfig = defaultCfg;
    if (!m_cache) {
        m_cache = new ::LLMCache(2000, 21600000);
    }
}

uint32_t DialogueEngine::hashPromptText(const std::string& text) const {
    return m_cache ? m_cache->hash_prompt("global", text, {}) : 0;
}

std::string DialogueEngine::generateConversationId() {
    return "conv_" + std::to_string(m_nextConvId++);
}

std::string DialogueEngine::getNpcName(const std::string& npcId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [id, conv] : m_conversations) {
        for (size_t i = 0; i < conv.participant_ids.size(); i++) {
            if (conv.participant_ids[i] == npcId)
                return conv.participant_names[i];
        }
    }
    return npcId;
}

std::string DialogueEngine::startConversation(const std::string& topic,
                                               const std::string& locationId,
                                               const std::vector<std::string>& participantIds,
                                               const std::vector<std::string>& participantNames,
                                               const ConversationConfig* cfg) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (participantIds.size() < 2) return "";

    std::string convId = generateConversationId();
    auto& conv = m_conversations[convId];
    conv.conversation_id = convId;
    conv.topic = topic;
    conv.location_id = locationId;
    conv.participant_ids = participantIds;
    conv.participant_names = participantNames;
    conv.config = cfg ? *cfg : m_defaultConfig;
    conv.current_speaker_index = 0;
    conv.turn_count = 0;
    conv.started_at = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    conv.last_turn_at = conv.started_at;
    conv.active = true;

    fprintf(stdout, "[DialogueEngine] Started conversation %s: topic='%s', %zu participants\n",
        convId.c_str(), topic.c_str(), participantIds.size());

    WorldStateManager::instance().record_event(
        "conversation_start",
        "Conversation started: " + topic,
        0.3f, {}, 0.7f);

    return convId;
}

bool DialogueEngine::advanceConversation(const std::string& conversationId) {
    ActiveConversation* convPtr = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_conversations.find(conversationId);
        if (it == m_conversations.end() || !it->second.active) return false;
        convPtr = &it->second;
    }

    auto& conv = *convPtr;

    if (conv.turn_count >= conv.config.max_turns) {
        fprintf(stdout, "[DialogueEngine] Conversation %s reached max turns (%d)\n",
            conversationId.c_str(), conv.config.max_turns);
        endConversation(conversationId);
        return false;
    }

    std::string speakerId = selectNextSpeaker(conv);
    if (speakerId.empty()) {
        fprintf(stdout, "[DialogueEngine] No speaker selected for %s\n", conversationId.c_str());
        endConversation(conversationId);
        return false;
    }

    std::string speakerName = getNpcName(speakerId);

    std::string listenerId;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (size_t i = 0; i < conv.participant_ids.size(); i++) {
            if (conv.participant_ids[i] != speakerId) {
                listenerId = conv.participant_ids[i];
                break;
            }
        }
    }

    if (listenerId.empty()) {
        endConversation(conversationId);
        return false;
    }

    std::string systemPrompt = buildSystemPrompt(conv);
    std::string userPrompt = buildUserPrompt(conv, speakerId, speakerName, listenerId);
    std::string fullPrompt = buildFullPrompt(conv, speakerId, speakerName, listenerId);

    fprintf(stdout, "[DialogueEngine] Turn %d: %s -> %s (prompt=%zu chars)\n",
        conv.turn_count + 1, speakerName.c_str(), getNpcName(listenerId).c_str(),
        fullPrompt.size());

    std::string responseText;
    bool usedCache = false;

    if (conv.config.use_cache && m_cache) {
        responseText = m_cache->try_get("global", fullPrompt, {});
        if (!responseText.empty()) {
            usedCache = true;
            fprintf(stdout, "[DialogueEngine] Cache hit for turn %d\n", conv.turn_count + 1);
        }
    }

    if (responseText.empty()) {
        ModelRegistry::instance().ensureModelForNpc(speakerId);

        auto& engine = ModelEngine::instance();
        npc::ModelParams npcParams = ModelRegistry::instance().getParamsForNpc(speakerId);
        responseText = engine.completion(systemPrompt, userPrompt, &npcParams);
    }

    if (conv.config.use_tools) {
        int toolRounds = 0;
        while (toolRounds < conv.config.max_tool_rounds) {
            std::string toolName, toolParams;
            if (!ApiToolRegistry::instance().parseToolCall(responseText, toolName, toolParams)) {
                break;
            }
            fprintf(stdout, "[DialogueEngine] Tool call detected: %s(%s)\n",
                toolName.c_str(), toolParams.c_str());

            auto toolResult = ApiToolRegistry::instance().executeWithRetry(toolName, toolParams);
            std::string toolFeedback = ApiToolRegistry::instance().formatToolResult(toolResult);

            std::string followUpPrompt = userPrompt
                + "\n\n[上一步你调用了工具: " + toolName + "]\n"
                + "工具返回结果:\n" + toolFeedback
                + "\n\n请根据上述工具返回的信息继续对话。";

            npc::ModelParams npcParams = ModelRegistry::instance().getParamsForNpc(speakerId);
            responseText = ModelEngine::instance().completion(systemPrompt, followUpPrompt, &npcParams);
            toolRounds++;
        }
    }

    if (!responseText.empty() && conv.config.use_cache && !usedCache && m_cache) {
        m_cache->set("global", fullPrompt, {}, responseText);
    }

    ConversationTurn turn;
    turn.turn_index = conv.turn_count;
    turn.speaker_id = speakerId;
    turn.speaker_name = speakerName;
    turn.listener_id = listenerId;
    turn.content = responseText;
    turn.emotion = "neutral";
    turn.emotion_intensity = 0.3f;
    turn.timestamp_generated = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        conv.history.push_back(turn);
        conv.turn_count++;
        conv.last_turn_at = turn.timestamp_generated;
    }

    auto& memory = NpcMemoryStore::instance();
    memory.remember(speakerId,
        "Said to " + getNpcName(listenerId) + ": " + responseText,
        0.4f, 0.0f, turn.emotion,
        {listenerId}, {conv.topic});

    WorldStateManager::instance().record_npc_dialog(
        speakerId, speakerName, listenerId, responseText, turn.emotion, turn.emotion_intensity);

    if (m_turnCallback) {
        m_turnCallback(conv, turn);
    }

    return true;
}

bool DialogueEngine::endConversation(const std::string& conversationId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_conversations.find(conversationId);
    if (it == m_conversations.end()) return false;

    it->second.active = false;
    fprintf(stdout, "[DialogueEngine] Ended conversation %s (%d turns)\n",
        conversationId.c_str(), it->second.turn_count);

    WorldStateManager::instance().record_event(
        "conversation_end",
        "Conversation ended after " + std::to_string(it->second.turn_count) + " turns",
        0.2f, {}, 0.5f);

    return true;
}

bool DialogueEngine::injectPlayerMessage(const std::string& conversationId,
                                          const std::string& speakerId,
                                          const std::string& speakerName,
                                          const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_conversations.find(conversationId);
    if (it == m_conversations.end() || !it->second.active) return false;

    auto& conv = it->second;
    ConversationTurn turn;
    turn.turn_index = conv.turn_count;
    turn.speaker_id = speakerId;
    turn.speaker_name = speakerName;
    turn.content = content;
    turn.timestamp_generated = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    conv.history.push_back(turn);
    conv.turn_count++;
    conv.last_turn_at = turn.timestamp_generated;

    auto& memory = NpcMemoryStore::instance();
    std::string otherNpc;
    for (const auto& pid : conv.participant_ids) {
        if (pid != speakerId) { otherNpc = pid; break; }
    }
    if (!otherNpc.empty()) {
        memory.remember(otherNpc,
            "Player said: " + content,
            0.7f, 0.0f, "surprise",
            {speakerId}, {conv.topic});

        memory.onNpcInteraction(speakerId, otherNpc);
    }

    return true;
}

ActiveConversation* DialogueEngine::getConversation(const std::string& conversationId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_conversations.find(conversationId);
    return it != m_conversations.end() ? &it->second : nullptr;
}

const ActiveConversation* DialogueEngine::getConversation(const std::string& conversationId) const {
    auto it = m_conversations.find(conversationId);
    return it != m_conversations.end() ? &it->second : nullptr;
}

std::vector<std::string> DialogueEngine::activeConversationIds() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> ids;
    for (const auto& [id, conv] : m_conversations) {
        if (conv.active) ids.push_back(id);
    }
    return ids;
}

void DialogueEngine::setTurnCallback(TurnCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_turnCallback = std::move(cb);
}

void DialogueEngine::setStopCondition(const std::string& conversationId, int maxTurns) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_conversations.find(conversationId);
    if (it != m_conversations.end()) {
        it->second.config.max_turns = maxTurns;
    }
}

std::string DialogueEngine::selectNextSpeaker(const ActiveConversation& conv) const {
    switch (conv.config.speaker_mode) {
        case SpeakerSelectionMode::RoundRobin:
            return nextSpeakerRoundRobin(conv);
        case SpeakerSelectionMode::EmotionDriven:
            return nextSpeakerEmotionDriven(conv);
        default:
            return nextSpeakerRoundRobin(conv);
    }
}

std::string DialogueEngine::nextSpeakerRoundRobin(const ActiveConversation& conv) const {
    if (conv.participant_ids.empty()) return "";
    size_t idx = conv.current_speaker_index % conv.participant_ids.size();
    return conv.participant_ids[idx];
}

std::string DialogueEngine::nextSpeakerEmotionDriven(const ActiveConversation& conv) const {
    if (conv.participant_ids.empty()) return "";

    float bestPriority = -1.0f;
    std::string bestSpeaker;

    for (const auto& npcId : conv.participant_ids) {
        float priority = getSpeakerPriority(conv, npcId);
        if (priority > bestPriority) {
            bestPriority = priority;
            bestSpeaker = npcId;
        }
    }

    return bestSpeaker.empty() ? nextSpeakerRoundRobin(conv) : bestSpeaker;
}

float DialogueEngine::getSpeakerPriority(const ActiveConversation& conv,
                                          const std::string& npcId) const {
    float priority = 0.5f;

    if (!conv.history.empty()) {
        const auto& last = conv.history.back();
        if (last.speaker_id == npcId) {
            priority -= 0.3f;
        }
        if (last.listener_id == npcId) {
            priority += 0.3f;
        }
    }

    auto stats = NpcMemoryStore::instance().getStats(npcId);
    if (stats.short_term_count > 5) priority += 0.1f;

    return std::clamp(priority, 0.0f, 1.0f);
}

std::string DialogueEngine::buildSystemPrompt(const ActiveConversation& conv) const {
    std::ostringstream oss;

    if (!conv.config.system_prompt_template.empty()) {
        oss << conv.config.system_prompt_template << "\n\n";
    }

    oss << "你是一个视觉小说中的角色，正在参与一场对话。\n";
    oss << "对话主题: " << conv.topic << "\n";
    oss << "地点: " << conv.location_id << "\n";
    oss << "参与角色: ";
    for (const auto& name : conv.participant_names) {
        oss << name << " ";
    }
    oss << "\n\n";

    oss << "规则:\n";
    oss << "- 只输出你的对话内容，不要包含描述或动作标记\n";
    oss << "- 保持角色个性一致\n";
    oss << "- 回复简洁，2-5句话\n";
    oss << "- 推动对话自然发展\n";

    if (conv.config.use_tools) {
        oss << "\n" << ApiToolRegistry::instance().getToolsPrompt() << "\n";
    }

    return oss.str();
}

std::string DialogueEngine::buildUserPrompt(const ActiveConversation& conv,
                                              const std::string& speakerId,
                                              const std::string& speakerName,
                                              const std::string& listenerId) const {
    std::ostringstream oss;

    oss << "现在你是 " << speakerName << "，你正在和 " << getNpcName(listenerId) << " 对话。\n\n";

    if (conv.config.use_memory) {
        auto& memory = NpcMemoryStore::instance();
        std::string recentMemories = memory.formatRecentMemories(speakerId, 200);
        if (!recentMemories.empty()) {
            oss << "你最近的记忆:\n" << recentMemories << "\n";
        }
    }

    if (conv.config.use_world_state) {
        auto& wsm = WorldStateManager::instance();
        auto flags = wsm.get_active_flags(speakerId);
        if (!flags.empty()) {
            oss << "当前世界状态:\n";
            for (const auto& f : flags) {
                oss << "- " << f.key << ": " << f.value << "\n";
            }
            oss << "\n";
        }
    }

    oss << "对话历史:\n";
    int historyCount = 0;
    int startIdx = std::max(0, static_cast<int>(conv.history.size()) - 6);
    for (int i = startIdx; i < static_cast<int>(conv.history.size()); i++) {
        const auto& turn = conv.history[i];
        oss << turn.speaker_name << ": " << turn.content << "\n";
        historyCount++;
    }

    if (historyCount == 0) {
        oss << "(对话刚开始，请 " << speakerName << " 先发言)\n";
    }

    oss << "\n" << speakerName << ": ";

    return oss.str();
}

std::string DialogueEngine::buildFullPrompt(const ActiveConversation& conv,
                                              const std::string& speakerId,
                                              const std::string& speakerName,
                                              const std::string& listenerId) const {
    std::string system = buildSystemPrompt(conv);
    std::string user = buildUserPrompt(conv, speakerId, speakerName, listenerId);
    return system + "\n" + user;
}

} // namespace npc
