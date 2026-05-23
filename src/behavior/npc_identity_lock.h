#pragma once
#include "npc_brain_types.h"
#include <string>
#include <vector>
#include <regex>
#include <cstdint>

class NPCBrainManager;

enum class ViolationType : uint8_t {
    identity, behavior, voice, emotion, knowledge
};

enum class ViolationSeverity : uint8_t {
    warning, violation, critical
};

struct ViolationItem {
    ViolationType type = ViolationType::identity;
    ViolationSeverity severity = ViolationSeverity::violation;
    std::string detail;
    std::string context_snippet;
};

struct ValidationResult {
    bool passed = true;
    std::vector<ViolationItem> violations;
    int total_score = 0;
};

struct VoiceCheckResult {
    bool matches = true;
    std::vector<std::string> issues;
};

struct ViolationRecord {
    std::string violation_id;
    std::string npc_id;
    std::string text;
    std::string corrected_text;
    std::vector<ViolationItem> violations;
    int64_t timestamp = 0;
    float lock_strength_after = 0.0f;
};

enum class EvolutionTrigger : uint8_t {
    tested_5_times, tested_20_times, challenged_3_times,
    world_event, relationship_change, manual
};

struct EvolutionResult {
    bool evolved = false;
    std::string old_statement;
    std::string new_statement;
    std::string reason;
};

class NPCIdentityLockEngine {
public:
    explicit NPCIdentityLockEngine(NPCBrainManager* brain_manager);

    ValidationResult validate_output(const std::string& npc_id,
                                     const std::string& text,
                                     const std::string& context = "");

    void check_assertions(const IdentityLock& lock,
                          const std::string& text,
                          ValidationResult& result);

    void check_behavior_boundaries(const IdentityLock& lock,
                                   const std::string& text,
                                   ValidationResult& result);

    void check_voice_lock(const IdentityLock& lock,
                          const std::string& text,
                          ValidationResult& result);

    void check_emotion_boundaries(const IdentityLock& lock,
                                  const NPCBrain& brain,
                                  const std::string& context,
                                  const std::string& text,
                                  ValidationResult& result);

    void check_knowledge_boundaries(const IdentityLock& lock,
                                    const std::string& text,
                                    ValidationResult& result);

    std::string generate_correction(const IdentityLock& lock,
                                     const ValidationResult& violations);

    void record_violations(const std::string& npc_id,
                           const std::string& text,
                           const ValidationResult& violations,
                           const std::string& corrected = "");

    void reinforce_assertion(const std::string& npc_id,
                             const std::string& assertion_id);

    EvolutionResult process_evolution(const std::string& npc_id,
                                       EvolutionTrigger trigger);

    std::string get_system_prompt_suffix(const std::string& npc_id);

    static std::regex build_negation_pattern(const std::string& statement);

    static std::string escape_regex(const std::string& s);

    float get_lock_strength(const std::string& npc_id) const;

private:
    NPCBrainManager* brain_manager_;

    static std::string generate_violation_id();
    static int64_t now_ms();

    void reduce_lock_strength(IdentityLock& lock, float amount);
    NPCBrain* get_brain(const std::string& npc_id) const;

    std::vector<ViolationRecord> violation_history_;
};
