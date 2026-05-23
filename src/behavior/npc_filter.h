#pragma once
#include "npc_brain_types.h"
#include <string>
#include <vector>
#include <regex>

struct FilterResult {
    enum Severity { PASS, WARN, HIGH, BLOCKED };
    Severity severity = PASS;
    std::string reason;
    std::string suggested_action;
    bool blocked = false;
};

enum class InfoCategory { DISCARD, CLASS_A, CLASS_B };

struct InfoFilterResult {
    InfoCategory category = InfoCategory::DISCARD;
    std::string content;
    std::string domain;
};

class NPCFilterEngine {
public:
    FilterResult filter_offline(const NPCBrain& brain, const std::string& input);
    InfoFilterResult classify_information(const NPCBrain& brain, const std::string& text);
    std::string build_filtered_system_prompt(const NPCBrain& brain, const FilterResult& filter);

private:
    static std::vector<std::string> split_sentences(const std::string& text);
    static bool is_small_talk(const std::string& sentence);
    static bool is_personal_info(const std::string& sentence);
    static bool is_within_knowledge(const NPCBrain& brain, const std::string& sentence);
    static bool is_forbidden_knowledge(const NPCBrain& brain, const std::string& sentence);
    static bool is_inducement(const std::string& sentence);

    FilterResult check_spoiler(const std::string& input);
    FilterResult check_inducement(const std::string& input);
    FilterResult check_meta(const std::string& input);
    FilterResult check_modern_tech(const std::string& input);
    FilterResult check_identity_lock(const NPCBrain& brain, const std::string& input);
};
