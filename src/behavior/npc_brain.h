#pragma once
#include "npc_brain_types.h"
#include "npc_architecture.h"
#include "npc_identity_lock.h"
#include <string>
#include <functional>
#include <memory>

namespace npc { class Database; }
class NPCEmotionService;

struct DecisionResult {
    std::string dominant_layer;
    float confidence = 0.0f;
    std::string response_intent;
    std::string inner_thought;
};

struct PromptConfig {
    std::string system_prompt;
    std::string user_message;
    int max_tokens = 512;
    float temperature = 0.7f;
};

struct MemoryConsolidationResult {
    int short_term_count = 0;
    int long_term_count = 0;
    int core_count = 0;
    int promoted_memories = 0;
    int forgotten_memories = 0;
};

struct NightConsolidationResult {
    int promoted_memories = 0;
    int new_reflections = 0;
    int resolved_ruminations = 0;
    std::string dream_fragment;
};

class NPCBrainManager {
public:
    NPCBrainManager();
    ~NPCBrainManager();

    void set_database(std::shared_ptr<npc::Database> db);

    NPCBrain init_brain(const std::string& npc_id,
                        const std::string& name,
                        const std::string& role,
                        const std::vector<std::string>& traits,
                        const std::string& speaking_style,
                        const std::vector<std::string>& values,
                        const std::string& background = "",
                        NPCType npc_type = NPCType::Ordinary);

    NPCBrain* load_brain(const std::string& npc_id);
    void save_brain(const std::string& npc_id);
    NPCBrain* get_brain(const std::string& npc_id);
    std::vector<std::string> get_all_brain_ids() const;
    std::unordered_map<std::string, NPCBrain>& get_brains() { return brains_; }

    DecisionResult process_message(const std::string& npc_id,
                                   const std::string& user_msg,
                                   const std::string& speaker_name = "你");

    PromptConfig build_prompt(const std::string& npc_id,
                              const std::string& user_msg,
                              const std::string& speaker_name = "你");

    void process_response(const std::string& npc_id,
                          const std::string& user_msg,
                          const std::string& response,
                          const std::string& speaker_name = "你");

    void update_emotion(NPCBrain& brain, const std::string& user_msg);
    void decay_emotions(NPCBrain& brain);

    void add_memory(NPCBrain& brain, const std::string& content,
                    MemoryType type, float importance = 0.5f);
    std::vector<BrainMemory> retrieve_memories(NPCBrain& brain,
                                                const std::string& query,
                                                int limit = 5);
    void consolidate_memories(NPCBrain& brain);

    MemoryConsolidationResult perform_daily_consolidation(const std::string& npc_id);
    NightConsolidationResult perform_night_consolidation(const std::string& npc_id);

    void record_experience(NPCBrain& brain,
                           const std::string& content,
                           float importance,
                           float emotional_impact,
                           const std::vector<std::string>& related_npcs = {},
                           const std::vector<std::string>& tags = {},
                           const std::string& source = "self_experience");

    void reconstruct_memory_with_mood(NPCBrain& brain,
                                      const std::string& memory_content,
                                      float mood_valence);

    void process_heard_fact(NPCBrain& brain,
                            const std::string& fact_content,
                            const std::string& domain,
                            float trust_in_source,
                            const std::string& speaker_name = "");

    std::string generate_memory_summary(NPCBrain& brain);

    void extract_knowledge_from_speech(const std::string& npc_id, const std::string& speech_content);

    void absorb_overheard_knowledge(NPCBrain& brain, const std::string& speech_content, const std::string& speaker_name);

    void update_relationship(NPCBrain& brain, const std::string& target_id,
                             float affection_delta = 0.0f,
                             float trust_delta = 0.0f);

    DecisionResult decide_response(NPCBrain& brain, const std::string& user_msg);
    std::string get_decision_layer_name(NPCBrain& brain) const;

    std::string validate_identity(NPCBrain& brain, const std::string& response);

    NPCArchitectureEngine& architecture_engine() { return arch_engine_; }
    NPCIdentityLockEngine& identity_lock_engine() { return lock_engine_; }

    std::string brain_to_json(const NPCBrain& brain);
    NPCBrain brain_from_json(const std::string& json);

private:
    std::unordered_map<std::string, NPCBrain> brains_;
    std::shared_ptr<npc::Database> db_;

    NPCArchitectureEngine arch_engine_{this};
    NPCIdentityLockEngine lock_engine_{this};

    std::string detect_user_emotion(const std::string& msg);
    float detect_intensity(const std::string& msg);

    float calc_memory_relevance(const BrainMemory& mem, const std::string& query);
};
