#pragma once

#include "npc_brain_types.h"

#include <string>
#include <vector>

struct LocalResponseResult {
    bool handled = false;
    std::string response;
    std::string response_type;
};

class LocalResponseEngine {
public:
    LocalResponseResult try_local_response(const NPCBrain& brain, const std::string& input);

    struct Stats {
        int total_attempts = 0;
        int local_hits = 0;
    };
    const Stats& get_stats() const { return stats_; }

private:
    Stats stats_;

    std::string detect_scenario(const NPCBrain& brain, const std::string& input);

    std::string respond_identity(const NPCBrain& brain);
    std::string respond_greeting(const NPCBrain& brain);
    std::string respond_weather(const NPCBrain& brain);
    std::string respond_agreement();
    std::string respond_memory_recall(const NPCBrain& brain, const std::string& input);
    std::string respond_farewell(const NPCBrain& brain);
    std::string respond_knowledge_query(const NPCBrain& brain, const std::string& input);

    std::string respond_memory_recall_from(
        const std::vector<BrainMemory>& memories,
        const std::string& input);

    static bool contains_any(const std::string& text, const std::vector<std::string>& keywords);
    static std::string trim(const std::string& s);
};
