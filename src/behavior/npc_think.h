#pragma once
#include "npc_brain_types.h"
#include "npc_architecture.h"
#include "npc_local_response.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

struct System1Result {
    std::string detected_emotion;
    float somatic_intensity = 0.0f;
    std::string auto_behavior;
    std::string analogous_memory;
};

struct ThoughtResult {
    bool should_speak = false;
    std::string raw_thought;
    std::string inner_thought;
    std::string decision;
    float confidence = 0.0f;
    std::string intended_action;
    std::string speech_intent;
    std::vector<BrainMemory> accessed_memories;
    std::string chain_of_thought;
    bool used_local_response = false;
    LocalResponseResult local_response;
};

class NPCThinkPipeline {
public:
    NPCThinkPipeline();

    ThoughtResult think(NPCBrain& brain, const std::string& input, const std::string& speaker_name = "你");

    System1Result run_system1(const NPCBrain& brain, const std::string& input);

    BehaviorPriorityResult compute_behavior_priority(const NPCBrain& brain, float context_pressure = 0.0f);

    std::vector<BrainMemory> find_analogous_memories(const NPCBrain& brain, const std::string& input, int max_count = 2);

    void apply_state_cascade(NPCBrain& brain);

    LocalResponseEngine& get_local_engine() { return local_engine_; }

    struct Stats { int total_thoughts = 0; int local_hits = 0; int total_memories_accessed = 0; };
    const Stats& get_stats() const { return stats_; }

private:
    LocalResponseEngine local_engine_;
    Stats stats_;

    struct EmotionTrigger {
        std::string emotion;
        std::vector<std::string> keywords;
        std::string auto_behavior;
    };
    static const std::vector<EmotionTrigger> EMOTION_TRIGGERS;

    static std::vector<std::string> get_priority_order(const NPCBrain& brain);

    static std::string jaccard_similarity_match(const std::vector<BrainMemory>& memories, const std::string& query);
};
