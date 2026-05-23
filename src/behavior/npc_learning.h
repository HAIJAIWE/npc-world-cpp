#pragma once
#include "npc_brain_types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct DissonanceResult {
    bool has_dissonance = false;
    std::string resolution;
    std::string narrative;
};

struct ObservationalResult {
    bool learned = false;
    float confidence = 0.0f;
};

struct SocialObservation {
    std::string source;
    std::string source_npc_id;
    std::string cause;
    std::string effect;
    std::string valence;
    float trust = 0.0f;
};

class NPCLearningEngine {
public:
    DissonanceResult process_new_knowledge(NPCBrain& brain,
                                           const std::string& domain,
                                           const std::string& content,
                                           const std::string& source,
                                           float confidence);

    ObservationalResult process_social_observation(NPCBrain& brain,
                                                    const SocialObservation& observation);

    float get_behavior_influence(const NPCBrain& brain,
                                 const std::string& domain);

    std::string rule_adjudicate(const NPCBrain& brain,
                                float new_confidence,
                                float old_confidence,
                                float old_confidence_value);

private:
    static std::string infer_domain(const std::string& content);
    static float keyword_similarity(const std::string& a, const std::string& b);
    static std::vector<std::string> extract_keywords(const std::string& text);

    static float get_source_weight(const std::string& source);

    static const std::unordered_map<std::string, std::vector<std::string>> DOMAIN_KEYWORDS;
};
