#include "npc_autonomous.h"
#include <algorithm>
#include <cmath>
#include <ctime>

static std::mt19937& rng_instance() {
    static std::mt19937 engine(static_cast<unsigned>(std::time(nullptr)));
    return engine;
}

static float random_float(float min = 0.0f, float max = 1.0f) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng_instance());
}

NPCAutonomousEngine::NPCAutonomousEngine(NPCBrainManager& brain_manager)
    : brain_manager_(brain_manager) {
}

void NPCAutonomousEngine::tick(NPCBrain& brain) {
    const std::string& npc_id = brain.npc_id;

    auto it = active_actions_.find(npc_id);
    if (it != active_actions_.end()) {
        ActiveAction& active = it->second;
        int64_t now = static_cast<int64_t>(std::time(nullptr));
        float elapsed = static_cast<float>(now - active.started_at);

        if (elapsed < active.action.duration) {
            return;
        }

        if (active.action.type == AutonomousActionType::Sleep) {
            perform_night_consolidation(brain);
        }

        active_actions_.erase(it);
    }

    bool has_active = active_actions_.find(npc_id) != active_actions_.end();
    drift_needs(brain, has_active);

    AutonomousAction action = select_action(brain);

    ActiveAction active;
    active.action = action;
    active.started_at = static_cast<int64_t>(std::time(nullptr));
    active_actions_[npc_id] = active;

    apply_action(brain, action);
}

AutonomousAction NPCAutonomousEngine::select_action(const NPCBrain& brain) {
    struct Candidate {
        AutonomousActionType type;
        float weight;
    };
    std::vector<Candidate> candidates;

    const auto& needs = brain.needs;

    struct NeedRule {
        std::function<bool(const AutonomousNeeds&)> condition;
        AutonomousActionType key;
    };

    static const NeedRule rules[] = {
        {[](const AutonomousNeeds& n) { return n.hunger > 60.0f; }, AutonomousActionType::Eat},
        {[](const AutonomousNeeds& n) { return n.fatigue > 70.0f; }, AutonomousActionType::Sleep},
        {[](const AutonomousNeeds& n) { return n.fatigue > 50.0f; }, AutonomousActionType::Rest},
        {[](const AutonomousNeeds& n) { return n.loneliness > 50.0f && n.boredom > 40.0f; }, AutonomousActionType::Socialize},
        {[](const AutonomousNeeds& n) { return n.loneliness > 40.0f; }, AutonomousActionType::Gaze},
        {[](const AutonomousNeeds& n) { return n.boredom > 50.0f; }, AutonomousActionType::Wander},
        {[](const AutonomousNeeds& n) { return n.stress > 50.0f; }, AutonomousActionType::Meditate},
    };

    for (const auto& rule : rules) {
        if (rule.condition(needs)) {
            auto it = NEED_ACTIONS.find(rule.key);
            if (it != NEED_ACTIONS.end()) {
                for (const auto& [action_type, weight] : it->second) {
                    candidates.push_back({action_type, weight});
                }
            }
        }
    }

    if (candidates.empty()) {
        for (const auto& idle : IDLE_ACTIONS) {
            candidates.push_back({idle.action, idle.weight});
        }
    }

    float total_weight = 0.0f;
    for (auto& c : candidates) {
        c.weight = apply_weight_modifiers(brain, c.type, c.weight);
        total_weight += c.weight;
    }

    AutonomousActionType selected = AutonomousActionType::Idle;
    if (total_weight > 0.0f) {
        float roll = random_float(0.0f, total_weight);
        for (const auto& c : candidates) {
            roll -= c.weight;
            if (roll <= 0.0f) {
                selected = c.type;
                break;
            }
        }
    }

    std::string target_npc_id;
    if (selected == AutonomousActionType::Socialize) {
        target_npc_id = pick_target_for_socialize(brain);
        if (target_npc_id.empty()) {
            selected = AutonomousActionType::Wander;
        }
    }

    AutonomousAction action;
    action.type = selected;
    action.description = random_description(selected, target_npc_id);
    action.thought = action_thought(action.description);
    action.target_npc_id = target_npc_id;

    auto dur_it = ACTION_DURATIONS.find(selected);
    if (dur_it != ACTION_DURATIONS.end()) {
        action.duration = dur_it->second;
    } else {
        action.duration = 30.0f;
    }

    return action;
}

void NPCAutonomousEngine::apply_action(NPCBrain& brain, const AutonomousAction& action) {
    brain.emotion.immediate_emotion.trigger = action.description;

    switch (action.type) {
        case AutonomousActionType::Eat:
            brain.mental_energy = std::min(1.0f, brain.mental_energy + 0.06f);
            brain.emotion.current_mood = "满足";
            brain.emotion.mood_intensity = 0.4f;
            break;
        case AutonomousActionType::Drink:
            brain.mental_energy = std::min(1.0f, brain.mental_energy + 0.03f);
            break;
        case AutonomousActionType::Rest:
            brain.mental_energy = std::min(1.0f, brain.mental_energy + 0.12f);
            brain.emotion.current_mood = "放松";
            brain.emotion.mood_intensity = 0.2f;
            break;
        case AutonomousActionType::Sleep:
            brain.mental_energy = std::min(1.0f, brain.mental_energy + 0.6f);
            break;
        case AutonomousActionType::Exercise:
            brain.mental_energy = std::max(0.0f, brain.mental_energy - 0.12f);
            brain.emotion.current_mood = "有活力";
            brain.emotion.mood_intensity = 0.4f;
            break;
        case AutonomousActionType::Socialize:
            brain.mental_energy = std::max(0.0f, brain.mental_energy - 0.06f);
            if (!action.target_npc_id.empty()) {
                brain_manager_.update_relationship(brain, action.target_npc_id, 2.0f, 1.0f);
            }
            break;
        case AutonomousActionType::Meditate:
            brain.mental_energy = std::min(1.0f, brain.mental_energy + 0.06f);
            brain.emotion.current_mood = "平静";
            brain.emotion.mood_intensity = 0.1f;
            break;
        case AutonomousActionType::Complain:
            brain.emotion.current_mood = "烦躁";
            brain.emotion.mood_intensity = 0.4f;
            break;
        default:
            break;
    }

    switch (action.type) {
        case AutonomousActionType::Reminisce:
            brain_manager_.add_memory(brain,
                "独自想起了一些往事",
                MemoryType::ShortTerm, 0.2f);
            break;
        case AutonomousActionType::Plan:
            brain_manager_.add_memory(brain,
                "在心里盘算着接下来的计划",
                MemoryType::ShortTerm, 0.3f);
            break;
        case AutonomousActionType::Complain:
            brain_manager_.add_memory(brain,
                "情绪不好，发泄了几句",
                MemoryType::Emotional, 0.2f);
            break;
        default:
            break;
    }

    auto& needs = brain.needs;
    switch (action.type) {
        case AutonomousActionType::Eat:
            needs.hunger = std::max(0.0f, needs.hunger - 40.0f);
            break;
        case AutonomousActionType::Drink:
            needs.hunger = std::max(0.0f, needs.hunger - 15.0f);
            break;
        case AutonomousActionType::Sleep:
            needs.fatigue = std::max(0.0f, needs.fatigue - 80.0f);
            break;
        case AutonomousActionType::Rest:
            needs.fatigue = std::max(0.0f, needs.fatigue - 30.0f);
            break;
        case AutonomousActionType::Socialize:
            needs.loneliness = std::max(0.0f, needs.loneliness - 30.0f);
            break;
        case AutonomousActionType::Exercise:
            needs.stress = std::max(0.0f, needs.stress - 25.0f);
            break;
        case AutonomousActionType::Meditate:
            needs.stress = std::max(0.0f, needs.stress - 35.0f);
            needs.boredom = std::max(0.0f, needs.boredom - 10.0f);
            break;
        case AutonomousActionType::Read:
            needs.boredom = std::max(0.0f, needs.boredom - 25.0f);
            break;
        case AutonomousActionType::Craft:
            needs.boredom = std::max(0.0f, needs.boredom - 20.0f);
            break;
        default:
            break;
    }
}

const ActiveAction* NPCAutonomousEngine::get_active_action(const std::string& npc_id) const {
    auto it = active_actions_.find(npc_id);
    if (it != active_actions_.end()) {
        return &it->second;
    }
    return nullptr;
}

void NPCAutonomousEngine::perform_night_consolidation(NPCBrain& brain) {
    brain_manager_.consolidate_memories(brain);

    brain.mental_energy = brain.max_mental_energy;

    bool had_important_memories = false;
    for (const auto& mem : brain.short_term_memory) {
        if (mem.importance > 0.6f) {
            had_important_memories = true;
            break;
        }
    }

    if (had_important_memories) {
        brain_manager_.add_memory(brain,
            "夜里做了个梦，梦到了最近发生的事",
            MemoryType::ShortTerm, 0.3f);
    }

    brain.needs.fatigue = std::max(0.0f, brain.needs.fatigue - 60.0f);
    brain.needs.stress = std::max(0.0f, brain.needs.stress - 20.0f);
}

void NPCAutonomousEngine::drift_needs(NPCBrain& brain, bool has_active_action) {
    auto& needs = brain.needs;

    needs.hunger = std::min(100.0f, needs.hunger + 1.5f + random_float(0.0f, 2.0f));
    needs.fatigue = std::min(100.0f, needs.fatigue + (has_active_action ? 2.0f : 0.5f) + random_float(0.0f, 1.5f));
    needs.loneliness = std::min(100.0f, needs.loneliness + 0.5f + random_float(0.0f, 3.0f));
    needs.boredom = std::min(100.0f, needs.boredom + (has_active_action ? 0.3f : 1.0f) + random_float(0.0f, 3.0f));
    needs.stress = std::min(100.0f, std::max(0.0f, needs.stress + (has_active_action ? 1.0f : -0.3f) + (random_float() - 0.5f) * 2.0f));
}

float NPCAutonomousEngine::apply_weight_modifiers(const NPCBrain& brain, AutonomousActionType action, float base_weight) {
    float modified = base_weight;

    switch (action) {
        case AutonomousActionType::Socialize:
            modified *= (0.5f + brain.personality.extraversion);
            break;
        case AutonomousActionType::Exercise:
            modified *= (0.5f + (1.0f - brain.personality.neuroticism));
            break;
        case AutonomousActionType::Meditate:
            modified *= (0.5f + brain.personality.openness);
            break;
        case AutonomousActionType::Read:
            modified *= (0.5f + brain.personality.conscientiousness);
            break;
        case AutonomousActionType::Craft:
            modified *= (0.5f + brain.personality.openness * 0.5f + brain.personality.conscientiousness * 0.5f);
            break;
        case AutonomousActionType::Complain:
            modified *= (0.5f + brain.personality.neuroticism);
            break;
        case AutonomousActionType::Wander:
            modified *= (0.5f + brain.personality.openness);
            break;
        default:
            break;
    }

    return std::max(0.01f, modified);
}

std::string NPCAutonomousEngine::pick_target_for_socialize(const NPCBrain& brain) {
    if (brain.relationships.empty()) {
        return {};
    }

    std::vector<std::pair<std::string, float>> candidates;
    for (const auto& [id, rel] : brain.relationships) {
        if (rel.affection > -20.0f) {
            candidates.emplace_back(id, rel.affection);
        }
    }

    if (candidates.empty()) {
        return {};
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    size_t top_n = std::min<size_t>(3, candidates.size());
    size_t pick = static_cast<size_t>(random_float(0.0f, static_cast<float>(top_n) - 0.001f));
    return candidates[pick].first;
}

std::mt19937& NPCAutonomousEngine::rng() {
    return rng_instance();
}

std::string NPCAutonomousEngine::random_description(AutonomousActionType type, const std::string& target_npc_id) {
    auto it = ACTION_DESCRIPTIONS.find(type);
    if (it == ACTION_DESCRIPTIONS.end() || it->second.empty()) {
        return "做着自己的事";
    }

    const auto& options = it->second;
    size_t idx = static_cast<size_t>(random_float(0.0f, static_cast<float>(options.size()) - 0.001f));
    std::string desc = options[idx];

    if (!target_npc_id.empty()) {
        size_t pos = desc.find("{target}");
        if (pos != std::string::npos) {
            desc.replace(pos, 8, target_npc_id);
        }
    }

    return desc;
}

std::string NPCAutonomousEngine::action_thought(const std::string& description) {
    return "[自主] " + description;
}