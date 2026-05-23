#include "npc_think.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <set>

const std::vector<NPCThinkPipeline::EmotionTrigger> NPCThinkPipeline::EMOTION_TRIGGERS = {
    {
        "恐惧",
        {"威胁", "杀", "打", "伤害", "危险", "死", "小心", "逃", "救命", "恐怖", "害怕"},
        "step_back"
    },
    {
        "愤怒",
        {"讨厌", "恨", "恶心", "烦", "滚", "可恶", "气死", "混蛋", "闭嘴", "够了", "怒"},
        "frown"
    },
    {
        "愉悦",
        {"喜欢", "爱", "谢谢", "开心", "棒", "好开心", "高兴", "太好了", "真棒", "赞", "美"},
        "smile"
    },
    {
        "悲伤",
        {"难过", "哭", "悲伤", "失落", "伤心", "痛苦", "泪", "遗憾", "唉", "心碎"},
        "look_down"
    },
    {
        "惊讶",
        {"哇", "真的", "没想到", "天哪", "竟然", "吃惊", "意外", "吓一跳", "不会吧", "怎么可能"},
        "eyes_widen"
    }
};

NPCThinkPipeline::NPCThinkPipeline() = default;

static bool contains_any_keyword(const std::string& text, const std::vector<std::string>& keywords) {
    for (const auto& kw : keywords) {
        if (text.find(kw) != std::string::npos) {
            return true;
        }
    }
    return false;
}

static float jaccard_similarity(const std::string& a, const std::string& b) {
    if (a.empty() && b.empty()) return 1.0f;
    if (a.empty() || b.empty()) return 0.0f;

    std::set<char> set_a(a.begin(), a.end());
    std::set<char> set_b(b.begin(), b.end());

    std::set<char> intersection;
    std::set_intersection(
        set_a.begin(), set_a.end(),
        set_b.begin(), set_b.end(),
        std::inserter(intersection, intersection.begin())
    );

    std::set<char> union_set;
    std::set_union(
        set_a.begin(), set_a.end(),
        set_b.begin(), set_b.end(),
        std::inserter(union_set, union_set.begin())
    );

    if (union_set.empty()) return 0.0f;
    return static_cast<float>(intersection.size()) / static_cast<float>(union_set.size());
}

System1Result NPCThinkPipeline::run_system1(const NPCBrain& brain, const std::string& input) {
    System1Result result;

    float best_intensity = 0.0f;
    std::string best_emotion;

    for (const auto& trigger : EMOTION_TRIGGERS) {
        float match_count = 0.0f;
        for (const auto& kw : trigger.keywords) {
            if (input.find(kw) != std::string::npos) {
                match_count += 1.0f;
            }
        }
        if (match_count > 0.0f) {
            float intensity = match_count / static_cast<float>(trigger.keywords.size());
            intensity = std::min(intensity * 2.0f, 1.0f);
            if (intensity > best_intensity) {
                best_intensity = intensity;
                best_emotion = trigger.emotion;
                result.auto_behavior = trigger.auto_behavior;
                result.detected_emotion = trigger.emotion;
            }
        }
    }

    if (best_intensity > 0.0f) {
        result.somatic_intensity = best_intensity;

        result.somatic_intensity += brain.emotion.temperament.emotional_reactivity * 0.15f;
        result.somatic_intensity = std::min(result.somatic_intensity, 1.0f);

        if (best_intensity > 0.3f) {
            for (const auto& mem : brain.long_term_memory) {
                float sim = jaccard_similarity(input, mem.content);
                if (sim > 0.15f) {
                    result.analogous_memory = mem.content.substr(0, 80);
                    break;
                }
            }
        }
    }

    if (result.detected_emotion.empty()) {
        result.detected_emotion = "平静";
        result.somatic_intensity = 0.1f;
        result.auto_behavior = "neutral";
    }

    return result;
}

std::vector<std::string> NPCThinkPipeline::get_priority_order(const NPCBrain& brain) {
    switch (brain.npc_type) {
        case NPCType::Ordinary:
        case NPCType::TraumatizedOrdinary:
        case NPCType::HalfAwakened:
            return {"instinct", "emotion", "personality", "rational", "persona"};

        case NPCType::Awakened:
        case NPCType::Divine:
            return {"rational", "personality", "emotion", "instinct", "persona"};

        case NPCType::RuthlessRuler:
        case NPCType::EthicalPragmatist:
            return {"rational", "persona", "instinct", "personality", "emotion"};

        case NPCType::IdealistMartyr:
        case NPCType::IntermittentIdealist:
            return {"personality", "rational", "emotion", "instinct", "persona"};

        case NPCType::Bestial:
            return {"instinct", "instinct", "instinct", "instinct", "instinct"};

        case NPCType::Antisocial:
        case NPCType::Pathological:
            return {"instinct", "persona", "emotion", "personality", "rational"};

        case NPCType::FateDominated:
            return {"emotion", "instinct", "personality", "rational", "persona"};

        default:
            return {"instinct", "emotion", "personality", "rational", "persona"};
    }
}

BehaviorPriorityResult NPCThinkPipeline::compute_behavior_priority(
        const NPCBrain& brain, float context_pressure) {

    BehaviorPriorityResult result;

    auto priority_order = get_priority_order(brain);
    float adjusted_pressure = context_pressure;

    adjusted_pressure += brain.emotion.stress_level * 0.5f;
    adjusted_pressure += (1.0f - brain.mental_energy) * 0.3f;
    adjusted_pressure = std::min(adjusted_pressure, 1.0f);

    std::unordered_map<std::string, float> base_scores;
    base_scores["instinct"]     = 0.5f;
    base_scores["emotion"]      = 0.5f;
    base_scores["personality"]  = 0.5f;
    base_scores["rational"]     = 0.5f;
    base_scores["persona"]      = 0.5f;

    for (int i = 0; i < static_cast<int>(priority_order.size()) && i < 5; ++i) {
        float positional_bonus = 1.0f - static_cast<float>(i) * 0.15f;
        base_scores[priority_order[i]] += positional_bonus * 0.3f;
    }

    if (adjusted_pressure > 0.5f) {
        float shift = (adjusted_pressure - 0.5f) * 2.0f * 0.25f;
        base_scores["instinct"] += shift;
        base_scores["emotion"]  += shift * 0.5f;
        base_scores["rational"] -= shift * 0.5f;
        base_scores["personality"] -= shift * 0.3f;
    }

    {
        float fatigue_bonus = brain.needs.fatigue * 0.2f;
        base_scores["instinct"] += fatigue_bonus;
        base_scores["rational"] -= fatigue_bonus * 0.5f;
    }

    float total = 0.0f;
    for (const auto& [layer, score] : base_scores) {
        total += std::max(0.0f, score);
    }
    if (total > 0.0f) {
        for (auto& [layer, score] : base_scores) {
            score = std::max(0.0f, score) / total;
        }
    }

    result.layer_scores = base_scores;

    std::string best_layer;
    float best_score = -1.0f;
    for (const auto& [layer, score] : base_scores) {
        if (score > best_score) {
            best_score = score;
            best_layer = layer;
        }
    }
    result.dominant_layer = best_layer;

    for (const auto& [layer, score] : base_scores) {
        if (score < 0.15f) {
            result.suppressed_layers.push_back(layer);
        }
    }

    {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Dominant: %s (score=%.2f), pressure=%.2f, stress=%.2f",
            best_layer.c_str(), best_score, adjusted_pressure, brain.emotion.stress_level);
        result.explanation = buf;
    }

    return result;
}

std::vector<BrainMemory> NPCThinkPipeline::find_analogous_memories(
        const NPCBrain& brain, const std::string& input, int max_count) {

    std::vector<std::pair<float, const BrainMemory*>> scored;

    for (const auto& mem : brain.long_term_memory) {
        if (mem.content.length() < 4) continue;

        float sim = jaccard_similarity(input, mem.content);
        float keyword_overlap = 0.0f;

        std::istringstream iss(input);
        std::string word;
        while (iss >> word) {
            if (mem.content.find(word) != std::string::npos) {
                keyword_overlap += 1.0f;
            }
        }

        float score = sim * 0.4f + keyword_overlap * 0.1f + mem.importance * 0.3f;
        float recency = 1.0f;
        if (mem.last_accessed > 0) {
            recency = std::min(1.0f, 1000.0f / static_cast<float>(mem.last_accessed + 1));
        }
        score += recency * 0.1f;

        if (score > 0.1f) {
            scored.emplace_back(score, &mem);
        }
    }

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<BrainMemory> results;
    int count = 0;
    for (const auto& [score, mem_ptr] : scored) {
        if (count >= max_count) break;
        BrainMemory copy = *mem_ptr;
        copy.importance = score;
        results.push_back(copy);
        ++count;
    }

    return results;
}

std::string NPCThinkPipeline::jaccard_similarity_match(
        const std::vector<BrainMemory>& memories, const std::string& query) {

    const BrainMemory* best = nullptr;
    float best_score = 0.0f;

    for (const auto& mem : memories) {
        float score = jaccard_similarity(query, mem.content);
        score += mem.importance * 0.2f;
        if (score > best_score) {
            best_score = score;
            best = &mem;
        }
    }

    if (best && best_score > 0.05f) {
        return best->content.substr(0, 120);
    }
    return "";
}

void NPCThinkPipeline::apply_state_cascade(NPCBrain& brain) {
    EmotionalState& emo = brain.emotion;
    BodyState& body = brain.body;
    AutonomousNeeds& needs = brain.needs;
    InternalConflictSystem& conflicts = brain.conflicts;

    float physiological_stress = 0.0f;

    if (body.stamina < 0.3f) {
        physiological_stress += (0.3f - body.stamina) * 2.0f * 0.3f;
    }

    if (body.pain_threshold > 0.7f) {
        physiological_stress += (body.pain_threshold - 0.7f) * 2.0f * 0.25f;
    }

    physiological_stress += needs.fatigue * 0.2f;
    physiological_stress += needs.hunger * 0.15f;

    float old_stress = emo.stress_level;
    emo.stress_level = std::min(1.0f, emo.stress_level + physiological_stress);

    if (needs.fatigue > 0.6f) {
        emo.temperament.emotional_reactivity =
            std::min(2.0f, emo.temperament.emotional_reactivity + 0.05f);
    }

    float emotional_mental_drain = 0.0f;
    if (emo.stress_level > 0.6f) {
        float stress_factor = (emo.stress_level - 0.6f) * 2.5f;
        emotional_mental_drain += stress_factor * 0.2f;
    }

    if (emo.immediate_emotion.intensity > 0.7f) {
        emotional_mental_drain += (emo.immediate_emotion.intensity - 0.7f) * 0.3f;
    }

    if (emo.background_mood.arousal > 0.8f) {
        emotional_mental_drain += (emo.background_mood.arousal - 0.8f) * 0.15f;
    }

    brain.mental_energy = std::max(0.0f, brain.mental_energy - emotional_mental_drain);

    float cognitive_fatigue = 0.0f;
    cognitive_fatigue += conflicts.rumination_intensity * 0.15f;
    cognitive_fatigue += conflicts.current_mental_drain * 0.1f;

    if (brain.mental_energy < 0.3f) {
        float energy_debt = (0.3f - brain.mental_energy) * 0.15f;
        cognitive_fatigue += energy_debt;
    }

    needs.fatigue = std::min(1.0f, needs.fatigue + cognitive_fatigue);

    if (brain.mental_energy < 0.2f) {
        float drain = (0.2f - brain.mental_energy) * 0.1f;
        emo.background_mood.valence -= drain;
        emo.background_mood.arousal -= drain * 0.5f;
    }
}

static const char* emotion_to_tone(const std::string& emotion) {
    if (emotion == "愉悦") return "愉悦地";
    if (emotion == "愤怒") return "愤怒地";
    if (emotion == "悲伤") return "低沉地";
    if (emotion == "恐惧") return "紧张地";
    if (emotion == "惊讶") return "惊讶地";
    return "平静地";
}

static std::string generate_inner_thought(
        const NPCBrain& brain,
        const std::string& input,
        const std::string& speaker_name,
        const System1Result& s1,
        const BehaviorPriorityResult& priority) {

    const auto& name = brain.personality.name;
    const auto& style = brain.personality.speaking_style;
    const std::string& emotion = s1.detected_emotion;
    const std::string& dominant = priority.dominant_layer;

    char buf[512];

    if (!s1.analogous_memory.empty()) {
        std::snprintf(buf, sizeof(buf),
            "%s %s说「%s」……这让我想起%s。%s",
            name.c_str(), emotion_to_tone(emotion),
            input.c_str(), s1.analogous_memory.c_str(),
            dominant == "instinct" ? "本能告诉我该有所反应。" :
            dominant == "emotion"  ? "现在不是理性思考的时候。" :
            dominant == "rational" ? "我需要冷静分析一下。" :
            "让我想想怎么回应。");
    } else {
        std::snprintf(buf, sizeof(buf),
            "%s %s听到%s说「%s」。%s",
            name.c_str(), emotion_to_tone(emotion),
            speaker_name.c_str(), input.c_str(),
            dominant == "instinct" ? "本能最先被触动。" :
            dominant == "emotion"  ? "情绪涌了上来。" :
            dominant == "personality" ? "这触动了我的信念。" :
            dominant == "rational" ? "我思考着该怎么应对。" :
            "我维持着一贯的做派。");
    }

    return buf;
}

static std::string generate_decision(
        const NPCBrain& brain,
        const System1Result& s1,
        const BehaviorPriorityResult& priority) {

    const std::string& dominant = priority.dominant_layer;

    if (dominant == "instinct") {
        if (s1.detected_emotion == "恐惧") return "本能后退，保持距离";
        if (s1.detected_emotion == "愤怒") return "准备反击或对峙";
        if (s1.detected_emotion == "愉悦") return "靠近，放松戒备";
        return "按照本能反应行动";
    }

    if (dominant == "emotion") {
        if (s1.detected_emotion == "悲伤") return "表达失落，寻求安慰";
        if (s1.detected_emotion == "愤怒") return "发泄情绪";
        if (s1.detected_emotion == "愉悦") return "分享快乐";
        if (s1.detected_emotion == "恐惧") return "寻求安全感";
        return "根据当前情绪回应";
    }

    if (dominant == "personality") {
        if (!brain.personality.values.empty()) {
            return "基于" + brain.personality.values[0] + "的信念做出回应";
        }
        return "按照一贯风格回应";
    }

    if (dominant == "rational") {
        return "权衡利弊后做出理性回应";
    }

    if (dominant == "persona") {
        return "维持人设，按身份要求回应";
    }

    return "做出回应";
}

static std::string decide_speech_intent(
        const std::string& decision,
        bool should_speak) {
    if (!should_speak) return "silent";

    if (decision.find("表达") != std::string::npos ||
        decision.find("分享") != std::string::npos) {
        return "express_emotion";
    }
    if (decision.find("反击") != std::string::npos ||
        decision.find("对峙") != std::string::npos) {
        return "confront";
    }
    if (decision.find("安慰") != std::string::npos ||
        decision.find("靠近") != std::string::npos) {
        return "approach";
    }
    if (decision.find("解释") != std::string::npos ||
        decision.find("分析") != std::string::npos ||
        decision.find("理性") != std::string::npos) {
        return "explain";
    }
    return "respond";
}

ThoughtResult NPCThinkPipeline::think(
        NPCBrain& brain,
        const std::string& input,
        const std::string& speaker_name) {

    stats_.total_thoughts++;
    ThoughtResult result;

    System1Result s1 = run_system1(brain, input);

    float context_pressure = brain.emotion.stress_level * 0.5f
                           + (1.0f - brain.mental_energy) * 0.3f
                           + brain.needs.fatigue * 0.2f;
    context_pressure = std::min(context_pressure, 1.0f);
    BehaviorPriorityResult priority = compute_behavior_priority(brain, context_pressure);

    std::vector<BrainMemory> memories = find_analogous_memories(brain, input);
    result.accessed_memories = memories;
    stats_.total_memories_accessed += static_cast<int>(memories.size());

    LocalResponseResult local = local_engine_.try_local_response(brain, input);
    if (local.handled) {
        stats_.local_hits++;
        result.used_local_response = true;
        result.local_response = local;
        result.should_speak = true;
        result.inner_thought = generate_inner_thought(brain, input, speaker_name, s1, priority);
        result.decision = "使用本地内置回答";
        result.speech_intent = local.response_type;
        result.confidence = 0.85f;

        char cot[1024];
        std::snprintf(cot, sizeof(cot),
            "[System 1] Detected emotion: %s (intensity=%.2f)\n"
            "[System 1] Somatic: %s\n"
            "[Priority] Dominant layer: %s\n"
            "[Priority] Scores: instinct=%.2f emotion=%.2f personality=%.2f rational=%.2f persona=%.2f\n"
            "[Local] Scenario: %s\n"
            "[Local] Response: %s\n"
            "[Decision] %s",
            s1.detected_emotion.c_str(), s1.somatic_intensity,
            s1.auto_behavior.c_str(),
            priority.dominant_layer.c_str(),
            priority.layer_scores.at("instinct"),
            priority.layer_scores.at("emotion"),
            priority.layer_scores.at("personality"),
            priority.layer_scores.at("rational"),
            priority.layer_scores.at("persona"),
            local.response_type.c_str(),
            local.response.c_str(),
            result.decision.c_str());
        result.chain_of_thought = cot;

        brain.total_decisions++;
        return result;
    }

    result.used_local_response = false;
    result.should_speak = true;

    result.inner_thought = generate_inner_thought(brain, input, speaker_name, s1, priority);
    result.decision = generate_decision(brain, s1, priority);
    result.speech_intent = decide_speech_intent(result.decision, result.should_speak);

    result.confidence = 0.5f + s1.somatic_intensity * 0.2f
                      + priority.layer_scores[priority.dominant_layer] * 0.15f
                      - brain.emotion.stress_level * 0.1f;
    result.confidence = std::max(0.1f, std::min(0.95f, result.confidence));

    {
        const std::string& dominant = priority.dominant_layer;
        std::string intended;
        if (dominant == "instinct") {
            intended = s1.detected_emotion == "恐惧" ? "step_back" :
                       s1.detected_emotion == "愤怒" ? "clench_fist" :
                       s1.detected_emotion == "愉悦" ? "step_forward" : "react";
        } else if (dominant == "emotion") {
            intended = "express_" + s1.detected_emotion;
        } else if (dominant == "personality") {
            intended = "act_according_to_values";
        } else if (dominant == "rational") {
            intended = "analyze_and_respond";
        } else {
            intended = "maintain_persona";
        }
        result.intended_action = intended;
    }

    {
        std::string memory_section;
        if (!memories.empty()) {
            memory_section = "[Memory] Found " + std::to_string(memories.size()) + " analogous memories\n";
            for (size_t i = 0; i < memories.size(); ++i) {
                memory_section += "  [" + std::to_string(i + 1) + "] " +
                                  memories[i].content.substr(0, 60) + "\n";
            }
        } else {
            memory_section = "[Memory] No analogous memories found\n";
        }

        char cot[2048];
        std::snprintf(cot, sizeof(cot),
            "[System 1] Detected emotion: %s (intensity=%.2f)\n"
            "[System 1] Somatic: %s\n"
            "[System 1] Analogous memory: %s\n"
            "[Priority] Dominant layer: %s\n"
            "[Priority] Scores: instinct=%.2f emotion=%.2f personality=%.2f rational=%.2f persona=%.2f\n"
            "[Priority] Suppressed layers: %zu\n"
            "%s"
            "[Cascade] mental_energy=%.2f stress=%.2f fatigue=%.2f\n"
            "[Inner Thought] %s\n"
            "[Decision] %s\n"
            "[Intended Action] %s",
            s1.detected_emotion.c_str(), s1.somatic_intensity,
            s1.auto_behavior.c_str(),
            s1.analogous_memory.empty() ? "none" : s1.analogous_memory.c_str(),
            priority.dominant_layer.c_str(),
            priority.layer_scores.at("instinct"),
            priority.layer_scores.at("emotion"),
            priority.layer_scores.at("personality"),
            priority.layer_scores.at("rational"),
            priority.layer_scores.at("persona"),
            priority.suppressed_layers.size(),
            memory_section.c_str(),
            brain.mental_energy, brain.emotion.stress_level, brain.needs.fatigue,
            result.inner_thought.c_str(),
            result.decision.c_str(),
            result.intended_action.c_str());
        result.chain_of_thought = cot;
    }

    brain.total_decisions++;
    return result;
}