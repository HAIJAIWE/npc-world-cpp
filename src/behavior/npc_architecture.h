#pragma once
#include "npc_brain_types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

class NPCBrainManager;

struct BehaviorContext {
    std::string situation;
    float pressure = 0.0f;
    std::string time_of_day;
};

struct AptitudeSet {
    float combat = 50.0f;
    float social = 50.0f;
    float craft = 50.0f;
    float scholar = 50.0f;
    float percept = 50.0f;
    float will = 50.0f;
};

struct ModuleStates {
    ModuleState body_instinct = ModuleState::Normal;
    ModuleState sensory_perception = ModuleState::Normal;
    ModuleState core_needs = ModuleState::Normal;
    ModuleState subconscious_defense = ModuleState::Normal;
    ModuleState memory_system = ModuleState::Normal;
    ModuleState self_cognition = ModuleState::Normal;
    ModuleState worldview_beliefs = ModuleState::Normal;
    ModuleState cognitive_biases = ModuleState::Normal;
    ModuleState habits_reflexes = ModuleState::Normal;
    ModuleState emotion_system = ModuleState::Normal;
    ModuleState persona_performance = ModuleState::Normal;
    ModuleState social_interaction = ModuleState::Normal;
    ModuleState dark_side = ModuleState::Normal;
    ModuleState internal_conflict = ModuleState::Normal;
    ModuleState self_deception = ModuleState::Normal;
};

struct ArchitectureCustomization {
    NPCType type = NPCType::Ordinary;
    std::string type_label;
    std::string description;
    ModuleStates modules;
    std::vector<std::string> decision_priority = {"instinct", "emotion", "personality", "rational", "persona"};
    bool can_break_cognition_loop = false;
    float fate_lock_strength = 85.0f;
    float unpredictable_variable_rate = 0.03f;
    AptitudeSet aptitudes;
};

struct BehaviorPriorityResult {
    std::string dominant_layer;
    std::unordered_map<std::string, float> layer_scores;
    std::vector<std::string> suppressed_layers;
    std::string explanation;
};

struct StateCascadeResult {
    float physiological_impact = 0.0f;
    std::string emotional_shift;
    float consciousness_delta = 0.0f;
    float decision_modifier = 0.0f;
    std::string cascade_path;
};

struct AwakeningResult {
    bool success = false;
    std::string stage;
    NPCType npc_type = NPCType::Ordinary;
    std::string target_type;
    std::string target_type_label;
    std::string message;
};

class NPCArchitectureEngine {
public:
    explicit NPCArchitectureEngine(NPCBrainManager* brain_manager);

    static const std::unordered_map<NPCType, ArchitectureCustomization>& get_presets();

    BehaviorPriorityResult compute_behavior_priority(
        const NPCBrain& brain,
        const ArchitectureCustomization& customization,
        const BehaviorContext& context);

    StateCascadeResult apply_state_cascade(
        const NPCBrain& brain,
        std::unordered_map<std::string, float>* layer_scores = nullptr);

    AwakeningResult attempt_awakening(const std::string& npc_id);

    static NPCType determine_awakening_type(const NPCBrain& brain);

    AwakeningResult force_awaken_to_type(const std::string& npc_id, NPCType target_type);

    static void apply_architecture_to_brain(NPCBrain& brain);

private:
    NPCBrainManager* brain_manager_;

    static float compute_instinct_score(const NPCBrain& brain);
    static float compute_emotion_score(const NPCBrain& brain);
    static float compute_personality_score(const NPCBrain& brain);
    static float compute_rational_score(const NPCBrain& brain);
    static float compute_persona_score(const NPCBrain& brain);

    static void apply_module_weights(
        std::unordered_map<std::string, float>& scores,
        const ModuleStates& modules);

    AwakeningResult awaken_to_half_awakened(const std::string& npc_id);
    AwakeningResult awaken_to_type(const std::string& npc_id, NPCType target_type);
};
