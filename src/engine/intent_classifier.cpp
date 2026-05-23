#include "engine/intent_classifier.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace npc {

const char* IntentResult::category_name() const {
    switch (category) {
        case IntentCategory::CHAT:           return "chat";
        case IntentCategory::QUERY_DATA:     return "query_data";
        case IntentCategory::EXECUTE_ACTION: return "execute_action";
        case IntentCategory::SYSTEM_COMMAND: return "system_command";
        case IntentCategory::TOOL_CALL:      return "tool_call";
        default: return "unknown";
    }
}

IntentClassifier& IntentClassifier::instance() {
    static IntentClassifier inst;
    return inst;
}

IntentClassifier::IntentClassifier() {
    buildDefaultRules();
}

void IntentClassifier::buildDefaultRules() {
    m_rules.clear();

    m_rules.push_back({IntentCategory::SYSTEM_COMMAND, "load_model",      "load", 0.9f});
    m_rules.push_back({IntentCategory::SYSTEM_COMMAND, "unload_model",    "unload", 0.9f});
    m_rules.push_back({IntentCategory::SYSTEM_COMMAND, "world_start",     "world_start", 0.9f});
    m_rules.push_back({IntentCategory::SYSTEM_COMMAND, "world_pause",     "world_pause", 0.9f});
    m_rules.push_back({IntentCategory::SYSTEM_COMMAND, "world_resume",    "world_resume", 0.9f});
    m_rules.push_back({IntentCategory::SYSTEM_COMMAND, "assign_npc_model", "assign_model", 0.9f});
    m_rules.push_back({IntentCategory::SYSTEM_COMMAND, "init_brain",      "init_brain", 0.9f});

    m_rules.push_back({IntentCategory::QUERY_DATA, "world_stats",    "stats", 0.8f});
    m_rules.push_back({IntentCategory::QUERY_DATA, "npc_list",       "list", 0.7f});
    m_rules.push_back({IntentCategory::QUERY_DATA, "get_time",       "time", 0.8f});
    m_rules.push_back({IntentCategory::QUERY_DATA, "get_weather",    "weather", 0.8f});
    m_rules.push_back({IntentCategory::QUERY_DATA, "get_location",   "location", 0.7f});
    m_rules.push_back({IntentCategory::QUERY_DATA, "where",          "where", 0.6f});
    m_rules.push_back({IntentCategory::QUERY_DATA, "who",            "who", 0.5f});
    m_rules.push_back({IntentCategory::QUERY_DATA, "what is",        "what_is", 0.5f});

    m_rules.push_back({IntentCategory::EXECUTE_ACTION, "delete",     "delete", 0.8f});
    m_rules.push_back({IntentCategory::EXECUTE_ACTION, "remove",     "remove", 0.8f});
    m_rules.push_back({IntentCategory::EXECUTE_ACTION, "create",     "create", 0.7f});
    m_rules.push_back({IntentCategory::EXECUTE_ACTION, "save",       "save", 0.7f});
    m_rules.push_back({IntentCategory::EXECUTE_ACTION, "move",       "move", 0.7f});
    m_rules.push_back({IntentCategory::EXECUTE_ACTION, "go to",      "go", 0.6f});
    m_rules.push_back({IntentCategory::EXECUTE_ACTION, "walk",       "walk", 0.6f});
    m_rules.push_back({IntentCategory::EXECUTE_ACTION, "run",        "run_script", 0.7f});
    m_rules.push_back({IntentCategory::EXECUTE_ACTION, "execute",    "execute", 0.7f});

    m_rules.push_back({IntentCategory::TOOL_CALL, "search",          "search", 0.8f});
    m_rules.push_back({IntentCategory::TOOL_CALL, "find",            "find", 0.7f});
    m_rules.push_back({IntentCategory::TOOL_CALL, "calculate",      "calculate", 0.7f});
    m_rules.push_back({IntentCategory::TOOL_CALL, "compute",        "compute", 0.7f});
    m_rules.push_back({IntentCategory::TOOL_CALL, "fetch",          "fetch", 0.8f});
    m_rules.push_back({IntentCategory::TOOL_CALL, "translate",      "translate", 0.7f});
    m_rules.push_back({IntentCategory::TOOL_CALL, "scan",           "scan", 0.6f});

    m_rules.push_back({IntentCategory::CHAT, "hello",     "greeting", 0.4f});
    m_rules.push_back({IntentCategory::CHAT, "你好",      "greeting", 0.4f});
    m_rules.push_back({IntentCategory::CHAT, "tell me",   "story", 0.3f});
    m_rules.push_back({IntentCategory::CHAT, "how are",   "greeting", 0.3f});
    m_rules.push_back({IntentCategory::CHAT, "think",     "opinion", 0.3f});
    m_rules.push_back({IntentCategory::CHAT, "feel",      "emotion", 0.3f});
    m_rules.push_back({IntentCategory::CHAT, "remember",  "memory", 0.3f});

    m_dirty = false;
}

float IntentClassifier::scoreMatch(const std::string& input, const PatternRule& rule) const {
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });

    std::string pat_lower = rule.pattern;
    std::transform(pat_lower.begin(), pat_lower.end(), pat_lower.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });

    if (lower.find(pat_lower) != std::string::npos) {
        return rule.weight;
    }
    return 0.0f;
}

IntentResult IntentClassifier::classify(const std::string& input) {
    if (m_dirty) buildDefaultRules();

    IntentResult best;
    float best_score = 0.15f;

    for (const auto& rule : m_rules) {
        float score = scoreMatch(input, rule);
        if (score > best_score) {
            best_score = score;
            best.category = rule.category;
            best.sub_category = rule.sub_category;
            best.confidence = score;
            best.matched_pattern = rule.pattern;
        }
    }

    m_stats.total++;

    if ((best.category == IntentCategory::SYSTEM_COMMAND && best.confidence >= 0.7f) ||
        (best.category == IntentCategory::QUERY_DATA     && best.confidence >= 0.7f) ||
        (best.category == IntentCategory::TOOL_CALL      && best.confidence >= 0.7f)) {
        best.should_route_directly = true;
        m_stats.local_routed++;
    }

    m_stats.by_category[static_cast<int>(best.category)]++;

    return best;
}

void IntentClassifier::addPattern(IntentCategory cat, const std::string& pattern, const std::string& sub) {
    m_rules.push_back({cat, sub.empty() ? pattern : sub, pattern, 0.8f});
    m_dirty = false;
}

void IntentClassifier::addKeyword(IntentCategory cat, const std::string& keyword, float weight) {
    m_rules.push_back({cat, keyword, keyword, weight});
    m_dirty = false;
}

IntentClassifier::Stats IntentClassifier::getStats() const {
    return m_stats;
}

void IntentClassifier::resetStats() {
    m_stats = Stats{};
}

} // namespace npc
