#pragma once
#include "npc_brain_types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct ImprovementIntent {
    std::string id;
    std::string pattern_type;
    std::string description;
    std::string goal;
    std::string motivation;
    float confidence = 20.0f;
    int successes = 0;
    int failures = 0;
    int attempts = 0;
    int times_observed = 0;
    bool active = true;
    int64_t created_at = 0;
    int64_t last_evaluated = 0;
};

struct HabitTracker {
    std::string intent_id;
    std::string habit_name;
    int consecutive_days = 0;
    std::string stage = "struggling";
};

class NPCImprovementEngine {
public:
    NPCImprovementEngine();

    std::vector<ImprovementIntent> detect_patterns(const NPCBrain& brain);

    ImprovementIntent form_improvement_intent(const NPCBrain& brain, const std::string& pattern_type, int times_observed);

    void evaluate_outcome(NPCBrain& brain, const std::string& npc_response, const std::string& context);

    const std::vector<ImprovementIntent>& get_intents(const std::string& npc_id) const;

private:
    std::unordered_map<std::string, std::vector<ImprovementIntent>> intents_by_npc_;
    std::unordered_map<std::string, std::vector<HabitTracker>> habits_by_npc_;

    bool detect_impulsive_pattern(const NPCBrain& brain, int& out_conflicts, int& out_regrets);
    bool detect_social_exhaustion(const NPCBrain& brain);
    bool detect_avoidance_pattern(const NPCBrain& brain, int& out_count);

    void update_habit(const std::string& npc_id, const std::string& intent_id, bool success);
    std::string get_habit_stage(int consecutive_days);

    std::string generate_id(const std::string& prefix);
};
