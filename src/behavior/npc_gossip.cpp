#include "npc_gossip.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <random>

static std::mt19937& rng_instance() {
    static std::mt19937 engine(static_cast<unsigned>(std::time(nullptr)));
    return engine;
}

static float random_float(float min = 0.0f, float max = 1.0f) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng_instance());
}

void NPCGossipEngine::init_from_brains(std::unordered_map<std::string, NPCBrain>& brains) {
    active_gossips_.clear();

    for (auto& [npc_id, brain] : brains) {
        for (const auto& knowledge : brain.knowledge) {
            if (knowledge.confidence <= 30.0f) continue;
            if (knowledge.source == "self_discovered") continue;

            GossipItem item;
            item.knowledge_id = knowledge.id;
            item.content = knowledge.content;
            item.domain = knowledge.domain;
            item.confidence = knowledge.confidence;
            item.source_npc_id = npc_id;
            item.created_at = knowledge.timestamp;

            active_gossips_.push_back(std::move(item));
        }
    }
}

void NPCGossipEngine::on_social_interaction(NPCBrain& speaker, NPCBrain& listener) {
    float trust = 30.0f;
    auto rel_it = speaker.relationships.find(listener.npc_id);
    if (rel_it != speaker.relationships.end()) {
        trust = rel_it->second.trust;
    }

    float spread_chance = trust / 100.0f * 0.6f;
    if (random_float() >= spread_chance) return;

    GossipItem* selected = find_spreadable_gossip(speaker.npc_id, listener.npc_id);
    if (!selected) return;

    float decayed_confidence = std::max(15.0f, selected->confidence - 10.0f);

    KnowledgeEntry entry;
    entry.id = "gossip_" + std::to_string(std::time(nullptr)) + "_" + listener.npc_id;
    entry.domain = selected->domain;
    entry.content = selected->content;
    entry.confidence = decayed_confidence;
    entry.source = "从" + speaker.personality.name + "听来";
    entry.timestamp = static_cast<int64_t>(std::time(nullptr));
    listener.knowledge.push_back(std::move(entry));

    selected->spread_to.insert(listener.npc_id);
    selected->confidence = decayed_confidence;

    BrainMemory mem;
    mem.id = "mem_" + std::to_string(std::time(nullptr)) + "_" + speaker.npc_id;
    mem.content = "我对" + listener.personality.name + "说漏嘴了：" + selected->content.substr(0, 40);
    mem.type = MemoryType::ShortTerm;
    mem.importance = 0.3f;
    mem.emotional_valence = 0.0f;
    mem.tags = {"流言", "社交"};
    mem.related_npc_ids = {listener.npc_id};
    mem.timestamp = static_cast<int64_t>(std::time(nullptr));
    speaker.short_term_memory.push_back(std::move(mem));

    NPCLearningEngine learning_engine;
    SocialObservation observation;
    observation.source = "gossip";
    observation.source_npc_id = speaker.npc_id;
    observation.cause = selected->content;
    observation.effect = speaker.personality.name + "传的流言";
    observation.valence = "negative";
    observation.trust = 0.0f;
    learning_engine.process_social_observation(listener, observation);
}

void NPCGossipEngine::spread_after_chat(
    const std::string& speaker_id,
    const std::vector<std::string>& listener_ids,
    std::unordered_map<std::string, NPCBrain>& brains)
{
    auto speaker_it = brains.find(speaker_id);
    if (speaker_it == brains.end()) return;

    NPCBrain& speaker = speaker_it->second;

    for (const auto& listener_id : listener_ids) {
        auto listener_it = brains.find(listener_id);
        if (listener_it == brains.end()) continue;

        on_social_interaction(speaker, listener_it->second);
    }
}

std::vector<GossipItem> NPCGossipEngine::get_gossips_for(const std::string& npc_id) const {
    std::vector<GossipItem> result;
    for (const auto& item : active_gossips_) {
        if (item.source_npc_id == npc_id) {
            result.push_back(item);
        }
    }
    return result;
}

bool NPCGossipEngine::can_spread_to(const GossipItem& item, const std::string& listener_id) const {
    return item.spread_to.find(listener_id) == item.spread_to.end();
}

GossipItem* NPCGossipEngine::find_spreadable_gossip(const std::string& speaker_id, const std::string& listener_id) {
    std::vector<size_t> candidates;
    for (size_t i = 0; i < active_gossips_.size(); ++i) {
        auto& item = active_gossips_[i];
        if (item.source_npc_id == speaker_id && can_spread_to(item, listener_id)) {
            candidates.push_back(i);
        }
    }

    if (candidates.empty()) return nullptr;

    size_t pick = candidates[static_cast<size_t>(random_float(0.0f, static_cast<float>(candidates.size()) - 0.001f))];
    return &active_gossips_[pick];
}