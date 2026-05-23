#pragma once
#include "npc_brain_types.h"
#include "npc_learning.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

struct GossipItem {
    std::string knowledge_id;
    std::string content;
    std::string domain;
    float confidence = 50.0f;
    std::string source_npc_id;
    int64_t created_at = 0;
    std::unordered_set<std::string> spread_to;
};

class NPCGossipEngine {
public:
    NPCGossipEngine() = default;

    void init_from_brains(std::unordered_map<std::string, NPCBrain>& brains);

    void on_social_interaction(NPCBrain& speaker, NPCBrain& listener);

    void spread_after_chat(const std::string& speaker_id,
                          const std::vector<std::string>& listener_ids,
                          std::unordered_map<std::string, NPCBrain>& brains);

    std::vector<GossipItem> get_gossips_for(const std::string& npc_id) const;

private:
    std::vector<GossipItem> active_gossips_;

    bool can_spread_to(const GossipItem& item, const std::string& listener_id) const;

    GossipItem* find_spreadable_gossip(const std::string& speaker_id, const std::string& listener_id);
};
