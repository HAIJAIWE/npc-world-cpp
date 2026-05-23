#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace npc {

enum class IntentCategory : uint8_t {
    CHAT,
    QUERY_DATA,
    EXECUTE_ACTION,
    SYSTEM_COMMAND,
    TOOL_CALL,
    UNKNOWN
};

struct IntentResult {
    IntentCategory category = IntentCategory::UNKNOWN;
    std::string sub_category;
    float confidence = 0.0f;
    std::string matched_pattern;
    bool should_route_directly = false;

    const char* category_name() const;
};

class IntentClassifier {
public:
    static IntentClassifier& instance();

    IntentResult classify(const std::string& input);

    void addPattern(IntentCategory cat, const std::string& pattern, const std::string& sub = "");
    void addKeyword(IntentCategory cat, const std::string& keyword, float weight = 1.0f);

    struct Stats {
        int64_t total = 0;
        int64_t local_routed = 0;
        int64_t by_category[6] = {};
    };
    Stats getStats() const;
    void resetStats();

private:
    IntentClassifier();

    struct PatternRule {
        IntentCategory category;
        std::string sub_category;
        std::string pattern;
        float weight = 1.0f;
    };

    std::vector<PatternRule> m_rules;
    Stats m_stats;
    bool m_dirty = false;

    void buildDefaultRules();
    float scoreMatch(const std::string& input, const PatternRule& rule) const;
};

} // namespace npc
