#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

enum class NPCType : uint8_t {
    Ordinary, Awakened, RuthlessRuler, IdealistMartyr,
    Antisocial, Pathological, FateDominated, HalfAwakened,
    EthicalPragmatist, IntermittentIdealist, TraumatizedOrdinary,
    Divine, Bestial
};

enum class ModuleState : uint8_t { Disabled, Weakened, Normal, Enhanced };

enum class MemoryType : uint8_t { ShortTerm, LongTerm, Core, Episodic, Semantic, Emotional };

struct ImmediateEmotion {
    std::string type;
    float intensity = 0.0f;
    std::string trigger;
    int64_t started_at = 0;
    float decay_rate = 0.01f;
    std::string raw_impulse;
};

struct BackgroundMood {
    float valence = 0.0f;
    float arousal = 0.0f;
    std::string label = "平静";
};

struct Temperament {
    float neuroticism_bias = 0.0f;
    float positivity_offset = 0.0f;
    float emotional_reactivity = 1.0f;
    float recovery_speed = 1.0f;
};

struct EmotionalState {
    std::string current_mood = "平静";
    float mood_intensity = 0.0f;
    float stress_level = 0.0f;
    float happiness_level = 0.0f;
    ImmediateEmotion immediate_emotion;
    BackgroundMood background_mood;
    Temperament temperament;
    std::string regulation_style = "reappraisal";
    float emotional_inertia = 0.0f;
};

struct PersonalityCore {
    std::string name;
    std::string role;
    std::vector<std::string> base_traits;
    std::vector<std::string> evolved_traits;
    std::string speaking_style;
    std::vector<std::string> values;
    std::vector<std::string> fears;
    std::vector<std::string> hopes;
    std::vector<std::string> beliefs;
    std::vector<std::string> quirks;
    std::vector<std::string> motivations;
    float openness = 0.5f;
    float conscientiousness = 0.5f;
    float extraversion = 0.5f;
    float agreeableness = 0.5f;
    float neuroticism = 0.5f;
};

struct BrainMemory {
    std::string id;
    std::string content;
    MemoryType type = MemoryType::ShortTerm;
    float importance = 0.5f;
    float emotional_valence = 0.0f;
    std::vector<std::string> tags;
    std::vector<std::string> related_npc_ids;
    int64_t timestamp = 0;
    int64_t last_accessed = 0;
    int access_count = 0;
};

struct Relationship {
    std::string target_id;
    std::string target_name;
    std::string type = "stranger";
    float affection = 0.0f;
    float trust = 0.0f;
    float respect = 0.0f;
    float familiarity = 0.0f;
    int64_t last_interaction = 0;
    int interaction_count = 0;
    float resentment = 0.0f;
    float jealousy = 0.0f;
    float dependency = 0.0f;
};

struct KnowledgeEntry {
    std::string id;
    std::string domain;
    std::string content;
    float confidence = 0.5f;
    std::string source;
    int64_t timestamp = 0;
    int64_t last_used = 0;
    int use_count = 0;
};

struct BeliefEntry {
    std::string id;
    std::string content;
    float confidence = 0.5f;
    std::string source;
    int64_t formed_at = 0;
    int challenged_count = 0;
};

struct ReflectionEntry {
    std::string id;
    std::string trigger;
    std::string old_belief;
    std::string new_insight;
    int64_t timestamp = 0;
    std::string impact = "minor";
};

struct CognitiveDissonance {
    std::string id;
    std::string new_knowledge;
    std::string conflict_content;
    float conflict_confidence = 0.0f;
    std::string resolution = "pending";
    std::string narrative;
    int64_t triggered_at = 0;
};

struct ObservationalLesson {
    std::string id;
    std::string source;
    std::string source_npc_id;
    std::string cause;
    std::string effect;
    std::string valence = "positive";
    float confidence = 0.5f;
    std::string domain;
    int64_t stored_at = 0;
};

struct Worldview {
    std::string world_nature = "mixed";
    std::string human_nature = "mixed";
    std::string justice = "exists";
    std::string fate = "mixed";
    std::string power = "earned";
    std::string knowledge = "relative";
    std::string change = "inevitable";
    std::string relationships = "cooperation";
    std::string description;
};

struct ValueSystem {
    struct Priority {
        std::string value;
        float weight = 0.5f;
        std::string reason;
    };
    struct MoralRedline {
        std::string rule;
        std::string reason;
        std::string flexibility = "absolute";
    };
    std::vector<Priority> priorities;
    std::vector<MoralRedline> moral_redlines;
    std::vector<std::string> taboos;
    std::vector<std::string> ideals;
};

struct AutonomousNeeds {
    float hunger = 0.0f;
    float fatigue = 0.0f;
    float loneliness = 0.0f;
    float boredom = 0.0f;
    float stress = 0.0f;
};

struct VoiceLock {
    std::vector<std::string> tone_range;
    std::vector<std::string> forbidden_tones;
    std::string vocabulary_style;
    std::string sentence_pattern;
    std::string catchphrase;
    std::string emotional_expression_style = "直接";
};

struct IdentityAssertion {
    std::string id;
    std::string statement;
    std::string type;
    std::string rigidity = "strong";
    std::string consequence;
};

struct BehaviorBoundary {
    std::string id;
    std::string forbidden_action;
    std::string reason;
    std::vector<std::string> exceptions;
    std::string violation_consequence;
};

struct IdentityLock {
    bool locked = false;
    float lock_strength = 0.7f;
    std::vector<IdentityAssertion> assertions;
    std::vector<BehaviorBoundary> behavior_boundaries;
    VoiceLock voice_lock;
    std::vector<std::string> immutable_facts;
    std::vector<std::string> core_values;
    std::vector<std::string> never_say_phrases;
    std::string identity_statement;
};

struct DarkSideState {
    float schadenfreude_level = 0.0f;
    float self_preservation_priority = 0.5f;
    float self_leniency = 0.7f;
    float others_strictness = 0.5f;
};

struct BodyState {
    struct StateOverride {
        float decision_penalty = 0.0f;
        float emotional_volatility = 0.0f;
        float irritability = 0.0f;
        float apathy = 0.0f;
    };
    float stamina = 1.0f;
    float pain_threshold = 0.5f;
    StateOverride sick;
    StateOverride sleep_deprived;
    StateOverride exhausted;
};

struct SubconsciousState {
    struct RepressedContent {
        std::string type;
        std::string content;
        float repression_level = 1.0f;
        std::vector<std::string> leak_triggers;
    };
    std::vector<RepressedContent> repressed_contents;
    float daydream_frequency = 0.3f;
};

struct SelfConcept {
    std::string self_image;
    std::string identity;
    std::string purpose;
    struct Goal {
        std::string goal;
        float priority = 0.5f;
        float progress = 0.0f;
    };
    std::vector<Goal> goals;
    std::vector<std::string> secrets;
    std::vector<std::string> regrets;
    std::vector<std::string> achievements;
};

struct InternalConflictPair {
    std::string a;
    std::string b;
    std::string current_dominant;
    float oscillation_frequency = 0.0f;
    std::string resolution_status = "unresolved";
};

struct InternalConflictSystem {
    std::vector<InternalConflictPair> core_conflict_pairs;
    float rumination_intensity = 0.0f;
    float current_mental_drain = 0.0f;
    std::string conflict_processing_style = "suppress";
};

struct FateLockSystem {
    float busyness_level = 0.5f;
    float comparison_obsession = 0.5f;
    float fear_of_standing_out = 0.5f;
    std::string fate_cycle_state = "complacent";
};

struct NPCBrain {
    std::string npc_id;
    NPCType npc_type = NPCType::Ordinary;
    int64_t created_at = 0;
    int64_t last_active = 0;
    int version = 1;
    int evolution_stage = 0;

    PersonalityCore personality;
    SelfConcept self_concept;
    IdentityLock identity_lock;

    Worldview worldview;
    ValueSystem value_system;
    std::vector<BeliefEntry> beliefs;
    AutonomousNeeds needs;
    SubconsciousState subconscious;
    DarkSideState dark_side;

    std::unordered_map<std::string, Relationship> relationships;

    std::vector<BrainMemory> short_term_memory;
    std::vector<BrainMemory> long_term_memory;
    std::vector<BrainMemory> core_memory;
    std::vector<KnowledgeEntry> knowledge;
    std::vector<ReflectionEntry> reflections;
    std::vector<CognitiveDissonance> cognitive_dissonances;
    std::vector<ObservationalLesson> observational_lessons;

    EmotionalState emotion;
    BodyState body;
    InternalConflictSystem conflicts;
    FateLockSystem fate_lock;
    float mental_energy = 1.0f;
    float max_mental_energy = 1.0f;
    int total_interactions = 0;
    int total_decisions = 0;
    int learning_episodes = 0;

    float wisdom = 50.0f;
    float selfAwareness = 30.0f;
};
