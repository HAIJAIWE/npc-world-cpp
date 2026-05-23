#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct TrainingRecord {
    std::string id;
    int64_t timestamp = 0;
    std::string npc_id;
    std::string npc_name;

    struct StateSnapshot {
        std::string immediate_emotion_type;
        float immediate_emotion_intensity = 0;
        std::string background_mood_label;
        float background_mood_valence = 0;
        float mood_intensity = 0;
        float mental_energy = 0;
        float system2_fatigue = 0;
        float stress_level = 0;
        std::string thought_stream;
        std::vector<std::string> unfinished_residue;
        std::string active_self;
        float performance_fatigue = 0;
        float biorhythm_energy = 0;
    } state;

    struct ConvContext {
        std::string topic;
        std::string situation;
        std::vector<std::string> recent_dialog;
        std::vector<std::string> other_npcs_present;
    } context;

    struct RelContext {
        std::string npc_name;
        float affection = 0;
        float trust = 0;
        float resentment = 0;
        float jealousy = 0;
    };
    std::vector<RelContext> relationship_context;

    std::vector<std::string> relevant_memories;

    std::string knowledge_hit;

    struct WorldCtx {
        std::string current_location;
        std::vector<std::string> active_plot_flags;
        std::string chapter_stage;
    } world;

    struct Sys1 {
        std::string gut_feeling;
        std::string somatic_marker;
        std::string pattern_match;
        std::vector<std::string> auto_behaviors;
    } system1;

    struct Sys2 {
        std::string inner_thought;
        std::string intended_action;
        bool should_speak = false;
        std::string speech_intent;
    } system2;

    struct ChainOfThought {
        std::string reasoning;
        std::vector<std::string> parsed_rules;
        std::string conflict_resolution;
        int64_t generated_at = 0;
    };
    std::vector<ChainOfThought> chain_of_thought;

    struct Output {
        std::string spoken_text;
        std::string mood_after;
        std::string local_or_llm;
    } output;
};

struct TrainingStats {
    int total = 0;
    std::unordered_map<std::string, int> by_npc;
    int llm_count = 0;
    int local_count = 0;
    double llm_rate = 0.0;
    double local_hit_rate = 0.0;
};

class TrainingDataLogger {
public:
    TrainingDataLogger() = default;
    ~TrainingDataLogger() = default;

    void log(const TrainingRecord& record);

    std::vector<TrainingRecord> get_records(const std::string& npc_id = "") const;

    TrainingStats get_stats() const;

    std::string export_jsonl() const;

    std::string export_json() const;

    void clear();

    size_t size() const { return records_.size(); }

private:
    static const size_t MAX_RECORDS = 2000;
    std::vector<TrainingRecord> records_;
};
