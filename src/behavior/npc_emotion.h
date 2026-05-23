#pragma once
#include "npc_brain_types.h"
#include "npc_brain.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <cstdint>
#include <functional>

struct EmotionTrigger {
    std::string type;
    float intensity = 0.0f;
};

enum class ReciprocityDirection : uint8_t {
    OwedToMe, IOweThem
};

struct ReciprocityEntry {
    std::string id;
    ReciprocityDirection direction = ReciprocityDirection::OwedToMe;
    std::string other_npc_id;
    std::string type;
    std::string description;
    float magnitude = 0.0f;
    bool repaid = false;
    int64_t timestamp = 0;
    float pressure = 0.0f;
};

struct ReciprocityLedger {
    std::vector<ReciprocityEntry> entries;
    float sensitivity_to_imbalance = 50.0f;
    float generosity_baseline = 40.0f;
    float grudge_retention = 35.0f;
};

struct PostInteractionRumination {
    std::string about_what;
    std::string rumination;
    bool resolved = false;
    int64_t started_at = 0;
};

struct SelfGapResult {
    float gap = 0.0f;
    std::string narrative;
};

class NPCEmotionService {
public:
    static NPCEmotionService& instance();

    void set_brain_manager(NPCBrainManager* mgr);

    void update_mental_energy(const std::string& npc_id, int64_t elapsed_ms, bool system2_used);

    void update_emotional_state(const std::string& npc_id, const EmotionTrigger* trigger, int64_t elapsed_ms);

    void update_biorhythm(const std::string& npc_id);

    void update_relationship_from_interaction(
        const std::string& npc_id,
        const std::string& target_id,
        const std::string& target_name,
        const std::string& interaction_type,
        float intensity);

    void trigger_post_interaction_rumination(
        const std::string& npc_id,
        const std::string& target_name,
        const std::string& interaction_type,
        float intensity);

    SelfGapResult evaluate_self_gap(const std::string& npc_id);

    void process_reciprocity(
        const std::string& npc_id,
        const std::string& other_npc_id,
        const std::string& event_type,
        const std::string& description,
        float magnitude);

private:
    NPCEmotionService() = default;
    ~NPCEmotionService() = default;
    NPCEmotionService(const NPCEmotionService&) = delete;
    NPCEmotionService& operator=(const NPCEmotionService&) = delete;

    NPCBrainManager* brain_mgr_ = nullptr;
    std::mt19937 rng_{ static_cast<unsigned>(std::time(nullptr)) };

    struct NPCEmotionData {
        ReciprocityLedger reciprocity_ledger;
        std::vector<PostInteractionRumination> ruminations;
        float system2_fatigue = 0.0f;
        float biorhythm_energy = 50.0f;
        float biorhythm_mood = 50.0f;
        std::string thought_current_thread;
        int64_t thought_thread_started_at = 0;
        float thought_thread_intensity = 0.0f;
        std::vector<std::string> emotional_residue;
        float present_discrepancy = 30.0f;
    };

    std::unordered_map<std::string, NPCEmotionData> emotion_data_;

    NPCEmotionData& data(const std::string& npc_id);
    NPCBrain* get_brain(const std::string& npc_id);

    std::mt19937& rng();

    std::string generate_id();
    std::string format_rumination_string(const std::string& tpl, const std::string& target_name);
    static float clamp(float value, float min_val, float max_val);
    static int clamp_int(int value, int min_val, int max_val);
};
