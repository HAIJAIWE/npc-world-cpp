#include "npc_architecture.h"
#include "npc_brain.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>

static std::string NPCTypeToStr(NPCType type);

static const std::unordered_map<NPCType, ArchitectureCustomization> s_presets = {
    {NPCType::Ordinary, {
        NPCType::Ordinary,
        "普通人",
        "15域完整基础架构，默认模块权重",
        {
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal
        },
        {"instinct", "emotion", "personality", "rational", "persona"},
        false, 85.0f, 0.03f,
        {50, 50, 50, 50, 50, 50}
    }},

    {NPCType::Awakened, {
        NPCType::Awakened,
        "完全觉醒者",
        "彻底击穿防觉醒封印，人生完全由自我意志主导",
        {
            ModuleState::Weakened, ModuleState::Enhanced, ModuleState::Weakened,
            ModuleState::Disabled, ModuleState::Normal, ModuleState::Enhanced,
            ModuleState::Disabled, ModuleState::Disabled, ModuleState::Normal,
            ModuleState::Weakened, ModuleState::Disabled, ModuleState::Weakened,
            ModuleState::Disabled, ModuleState::Disabled, ModuleState::Disabled
        },
        {"rational", "personality", "emotion", "instinct", "persona"},
        true, 0.0f, 0.15f,
        {50, 50, 50, 80, 85, 90}
    }},

    {NPCType::RuthlessRuler, {
        NPCType::RuthlessRuler,
        "极致利己的规则掌控者",
        "看透世俗规则并反向利用，利益、掌控、权力是唯一核心驱动力",
        {
            ModuleState::Normal, ModuleState::Enhanced, ModuleState::Enhanced,
            ModuleState::Weakened, ModuleState::Normal, ModuleState::Enhanced,
            ModuleState::Disabled, ModuleState::Weakened, ModuleState::Normal,
            ModuleState::Weakened, ModuleState::Disabled, ModuleState::Enhanced,
            ModuleState::Enhanced, ModuleState::Disabled, ModuleState::Weakened
        },
        {"rational", "persona", "instinct", "personality", "emotion"},
        true, 10.0f, 0.02f,
        {60, 88, 40, 82, 75, 55}
    }},

    {NPCType::IdealistMartyr, {
        NPCType::IdealistMartyr,
        "极致理想主义者/殉道者",
        "精神信念权重拉满，完全压制生物本能，可为信念牺牲生命",
        {
            ModuleState::Weakened, ModuleState::Enhanced, ModuleState::Weakened,
            ModuleState::Weakened, ModuleState::Enhanced, ModuleState::Enhanced,
            ModuleState::Enhanced, ModuleState::Weakened, ModuleState::Normal,
            ModuleState::Weakened, ModuleState::Disabled, ModuleState::Weakened,
            ModuleState::Disabled, ModuleState::Disabled, ModuleState::Disabled
        },
        {"personality", "rational", "emotion", "instinct", "persona"},
        true, 5.0f, 0.10f,
        {40, 25, 35, 88, 60, 95}
    }},

    {NPCType::Antisocial, {
        NPCType::Antisocial,
        "反社会/边缘型极端人格",
        "完全脱离规则约束，以打破规则为乐，无愧疚无共情",
        {
            ModuleState::Normal, ModuleState::Normal, ModuleState::Enhanced,
            ModuleState::Disabled, ModuleState::Normal, ModuleState::Weakened,
            ModuleState::Disabled, ModuleState::Enhanced, ModuleState::Normal,
            ModuleState::Weakened, ModuleState::Enhanced, ModuleState::Disabled,
            ModuleState::Enhanced, ModuleState::Disabled, ModuleState::Disabled
        },
        {"instinct", "persona", "emotion", "personality", "rational"},
        false, 0.0f, 0.05f,
        {78, 15, 45, 30, 40, 70}
    }},

    {NPCType::Pathological, {
        NPCType::Pathological,
        "病理性特殊心智者",
        "底层硬件完全重构，极端天赋与缺陷并存",
        {
            ModuleState::Normal, ModuleState::Enhanced, ModuleState::Normal,
            ModuleState::Weakened, ModuleState::Enhanced, ModuleState::Weakened,
            ModuleState::Weakened, ModuleState::Enhanced, ModuleState::Enhanced,
            ModuleState::Weakened, ModuleState::Disabled, ModuleState::Disabled,
            ModuleState::Weakened, ModuleState::Enhanced, ModuleState::Weakened
        },
        {"instinct", "emotion", "personality", "rational", "persona"},
        false, 0.0f, 0.12f,
        {20, 10, 88, 88, 25, 15}
    }},

    {NPCType::FateDominated, {
        NPCType::FateDominated,
        "极端命运变量主导者",
        "人生完全被极端随机变量主导，脱离努力-反馈闭环",
        {
            ModuleState::Normal, ModuleState::Normal, ModuleState::Weakened,
            ModuleState::Disabled, ModuleState::Enhanced, ModuleState::Weakened,
            ModuleState::Disabled, ModuleState::Enhanced, ModuleState::Normal,
            ModuleState::Enhanced, ModuleState::Disabled, ModuleState::Weakened,
            ModuleState::Weakened, ModuleState::Enhanced, ModuleState::Disabled
        },
        {"emotion", "instinct", "personality", "rational", "persona"},
        false, 0.0f, 0.08f,
        {45, 40, 50, 35, 30, 10}
    }},

    {NPCType::HalfAwakened, {
        NPCType::HalfAwakened,
        "半觉醒半沉沦的普通人",
        "看透NPC剧本但没有跳出的勇气，间歇性清醒持续性沉沦",
        {
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Weakened, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Normal, ModuleState::Enhanced, ModuleState::Normal
        },
        {"instinct", "emotion", "personality", "rational", "persona"},
        false, 60.0f, 0.06f,
        {45, 45, 50, 60, 65, 35}
    }},

    {NPCType::EthicalPragmatist, {
        NPCType::EthicalPragmatist,
        "有道德底线的世俗利己者",
        "在利己和道德之间找平衡，底线之上全力争取",
        {
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Normal, ModuleState::Normal, ModuleState::Enhanced,
            ModuleState::Weakened, ModuleState::Normal, ModuleState::Normal
        },
        {"rational", "personality", "emotion", "instinct", "persona"},
        false, 50.0f, 0.04f,
        {55, 82, 55, 60, 50, 50}
    }},

    {NPCType::IntermittentIdealist, {
        NPCType::IntermittentIdealist,
        "间歇性理想主义者",
        "在理想和现实之间拉扯，顺境理想拉满逆境现实拉满",
        {
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Enhanced, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Enhanced, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Weakened, ModuleState::Enhanced, ModuleState::Normal
        },
        {"instinct", "emotion", "personality", "rational", "persona"},
        false, 55.0f, 0.07f,
        {40, 50, 60, 78, 45, 30}
    }},

    {NPCType::TraumatizedOrdinary, {
        NPCType::TraumatizedOrdinary,
        "创伤后异化的边缘普通人",
        "经历过重大创伤，破碎但仍在世俗循环里活着",
        {
            ModuleState::Normal, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Enhanced, ModuleState::Enhanced, ModuleState::Weakened,
            ModuleState::Weakened, ModuleState::Normal, ModuleState::Normal,
            ModuleState::Enhanced, ModuleState::Normal, ModuleState::Weakened,
            ModuleState::Weakened, ModuleState::Enhanced, ModuleState::Normal
        },
        {"instinct", "emotion", "personality", "rational", "persona"},
        false, 70.0f, 0.05f,
        {40, 25, 55, 35, 20, 15}
    }},

    {NPCType::Divine, {
        NPCType::Divine,
        "神性存在",
        "彻底超越人性的天花板，无自我无执念无分别心",
        {
            ModuleState::Disabled, ModuleState::Enhanced, ModuleState::Disabled,
            ModuleState::Disabled, ModuleState::Weakened, ModuleState::Disabled,
            ModuleState::Disabled, ModuleState::Disabled, ModuleState::Weakened,
            ModuleState::Weakened, ModuleState::Disabled, ModuleState::Disabled,
            ModuleState::Disabled, ModuleState::Disabled, ModuleState::Disabled
        },
        {"rational", "personality", "emotion", "instinct", "persona"},
        true, 0.0f, 0.30f,
        {90, 88, 90, 95, 98, 100}
    }},

    {NPCType::Bestial, {
        NPCType::Bestial,
        "兽性存在",
        "完全退化为纯本能驱动的碳基生物",
        {
            ModuleState::Enhanced, ModuleState::Normal, ModuleState::Enhanced,
            ModuleState::Disabled, ModuleState::Disabled, ModuleState::Disabled,
            ModuleState::Disabled, ModuleState::Disabled, ModuleState::Enhanced,
            ModuleState::Disabled, ModuleState::Disabled, ModuleState::Disabled,
            ModuleState::Disabled, ModuleState::Disabled, ModuleState::Disabled
        },
        {"instinct", "instinct", "instinct", "instinct", "instinct"},
        false, 0.0f, 0.0f,
        {95, 5, 10, 5, 60, 15}
    }}
};

NPCArchitectureEngine::NPCArchitectureEngine(NPCBrainManager* brain_manager)
    : brain_manager_(brain_manager) {}

const std::unordered_map<NPCType, ArchitectureCustomization>&
NPCArchitectureEngine::get_presets() {
    return s_presets;
}

BehaviorPriorityResult NPCArchitectureEngine::compute_behavior_priority(
    const NPCBrain& brain,
    const ArchitectureCustomization& customization,
    const BehaviorContext& context)
{
    std::unordered_map<std::string, float> layer_scores;
    layer_scores["instinct"]    = compute_instinct_score(brain);
    layer_scores["emotion"]     = compute_emotion_score(brain);
    layer_scores["personality"] = compute_personality_score(brain);
    layer_scores["rational"]    = compute_rational_score(brain);
    layer_scores["persona"]     = compute_persona_score(brain);

    apply_module_weights(layer_scores, customization.modules);

    auto cascade = apply_state_cascade(brain, &layer_scores);

    float pressure = context.pressure;
    layer_scores["instinct"] += pressure * 0.3f;
    layer_scores["emotion"]  += pressure * 0.2f;
    if (pressure > 70.0f) {
        layer_scores["rational"] -= 15.0f;
    }

    if (context.time_of_day == "深夜" || context.time_of_day == "凌晨") {
        layer_scores["rational"] -= 10.0f;
        layer_scores["instinct"] += 5.0f;
    }

    for (auto& [layer, score] : layer_scores) {
        score = std::clamp(score, 0.0f, 100.0f);
    }

    std::vector<std::pair<std::string, float>> scored;
    for (const auto& [layer, score] : layer_scores) {
        scored.emplace_back(layer, score);
    }
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    const std::string& dominant_layer = scored[0].first;
    float dominant_score = scored[0].second;

    std::vector<std::string> suppressed;
    for (size_t i = 1; i < scored.size(); ++i) {
        if (scored[i].second < dominant_score * 0.6f) {
            suppressed.push_back(scored[i].first);
        }
    }

    static const std::unordered_map<std::string, std::string> explanations = {
        {"instinct",    "本能/潜意识主导决策"},
        {"emotion",     "情绪/瞬时状态主导决策"},
        {"personality", "人格/三观主导决策"},
        {"rational",    "理性/认知主导决策"},
        {"persona",     "人设/表演主导决策"}
    };
    auto expl_it = explanations.find(dominant_layer);
    std::string explanation = (expl_it != explanations.end())
        ? expl_it->second : "未知决策层";

    return {dominant_layer, std::move(layer_scores), std::move(suppressed), std::move(explanation)};
}

float NPCArchitectureEngine::compute_instinct_score(const NPCBrain& brain) {
    float consc = brain.personality.conscientiousness;
    float instant_grat = (1.0f - consc) * 100.0f + brain.needs.boredom * 30.0f;
    float delayed_grat = consc * 100.0f;
    float survival_drv = (1.0f - brain.body.stamina) * 50.0f + brain.needs.hunger * 50.0f;
    float max_frust    = brain.needs.stress * 100.0f;

    float score = 50.0f
        + instant_grat * 0.2f
        + (100.0f - delayed_grat) * 0.2f
        + survival_drv * 0.15f
        + max_frust * 0.2f;

    return std::clamp(score, 10.0f, 100.0f);
}

float NPCArchitectureEngine::compute_emotion_score(const NPCBrain& brain) {
    float mood_intensity_scaled = brain.emotion.mood_intensity * 10.0f;
    float score = 30.0f
        + mood_intensity_scaled * 5.0f
        + brain.conflicts.current_mental_drain * 0.2f;
    return std::clamp(score, 10.0f, 100.0f);
}

float NPCArchitectureEngine::compute_personality_score(const NPCBrain& brain) {
    float willpower = brain.personality.conscientiousness * 100.0f;
    float score = 40.0f
        + brain.selfAwareness * 0.25f
        + brain.wisdom * 0.3f
        + willpower * 0.1f;
    return std::clamp(score, 10.0f, 100.0f);
}

float NPCArchitectureEngine::compute_rational_score(const NPCBrain& brain) {
    float consciousness_level = (brain.wisdom + brain.selfAwareness) * 0.5f;
    float score = 35.0f
        + consciousness_level * 0.4f
        + brain.wisdom * 0.3f
        - brain.conflicts.current_mental_drain * 0.3f;

    const auto& traits = brain.personality.base_traits;
    bool has_rational_trait = false;
    const char* rational_keywords[] = {"理性", "冷静", "逻辑"};
    for (const auto& t : traits) {
        for (const auto& kw : rational_keywords) {
            if (t.find(kw) != std::string::npos) {
                has_rational_trait = true;
                break;
            }
        }
        if (has_rational_trait) break;
    }
    if (has_rational_trait) {
        score += 15.0f;
    }

    return std::clamp(score, 10.0f, 100.0f);
}

float NPCArchitectureEngine::compute_persona_score(const NPCBrain& brain) {
    float face_saving = brain.fate_lock.comparison_obsession * 100.0f;
    float fear_standing = brain.fate_lock.fear_of_standing_out * 100.0f;
    float social_trauma_count = 0.0f;

    float score = 20.0f
        + face_saving * 0.3f
        + fear_standing * 0.3f
        + social_trauma_count * 3.0f;
    return std::clamp(score, 5.0f, 100.0f);
}

void NPCArchitectureEngine::apply_module_weights(
    std::unordered_map<std::string, float>& scores,
    const ModuleStates& modules)
{
    struct LayerModule {
        const char* layer;
        ModuleState ModuleStates::*module_ptr;
        bool reversed;
    };

    const LayerModule map[] = {
        {"instinct",    &ModuleStates::body_instinct,      false},
        {"emotion",     &ModuleStates::emotion_system,     false},
        {"personality", &ModuleStates::self_cognition,     false},
        {"rational",    &ModuleStates::cognitive_biases,   true},
        {"persona",     &ModuleStates::persona_performance,false},
    };

    for (const auto& entry : map) {
        ModuleState state = modules.*(entry.module_ptr);
        float& score = scores[entry.layer];

        if (entry.reversed) {
            switch (state) {
                case ModuleState::Disabled: score *= 1.5f; break;
                case ModuleState::Weakened: score *= 1.2f; break;
                case ModuleState::Normal:   break;
                case ModuleState::Enhanced: score *= 0.5f; break;
            }
        } else {
            switch (state) {
                case ModuleState::Disabled: score *= 0.1f; break;
                case ModuleState::Weakened: score *= 0.5f; break;
                case ModuleState::Normal:   break;
                case ModuleState::Enhanced: score *= 1.5f; break;
            }
        }
    }
}

StateCascadeResult NPCArchitectureEngine::apply_state_cascade(
    const NPCBrain& brain,
    std::unordered_map<std::string, float>* layer_scores)
{
    StateCascadeResult result;
    const auto& body = brain.body;

    float impact = 0.0f;

    if (body.stamina < 0.3f) {
        impact += 10.0f;
    }

    if (body.pain_threshold < 0.3f) {
        impact += 15.0f;
    }

    if (brain.emotion.stress_level > 0.7f) {
        impact += 8.0f;
    }

    result.physiological_impact = impact;

    if (impact > 20.0f) {
        result.emotional_shift = "波动加剧";
    } else if (impact > 10.0f) {
        result.emotional_shift = "轻微波动";
    } else {
        result.emotional_shift = "稳定";
    }

    result.consciousness_delta = -(impact * 0.3f);
    result.decision_modifier   = -(impact * 0.5f);

    if (impact > 30.0f) {
        result.cascade_path = "生理状态严重扰动 → 情绪大幅波动 → 意识模糊 → 决策严重偏离";
    } else if (impact > 15.0f) {
        result.cascade_path = "生理状态中等扰动 → 情绪波动 → 意识轻度受损 → 决策偏低";
    } else {
        result.cascade_path = "生理状态正常 → 情绪稳定 → 意识清醒 → 决策正常";
    }

    if (layer_scores) {
        (*layer_scores)["rational"]  += -impact * 0.3f;
        (*layer_scores)["emotion"]   +=  impact * 0.4f;
        (*layer_scores)["instinct"]  +=  impact * 0.2f;
    }

    return result;
}

AwakeningResult NPCArchitectureEngine::attempt_awakening(const std::string& npc_id) {
    if (!brain_manager_) {
        return {false, "dormant", NPCType::Ordinary, "", "", "NPCBrainManager不可用"};
    }

    NPCBrain* brain = brain_manager_->get_brain(npc_id);
    if (!brain) {
        return {false, "dormant", NPCType::Ordinary, "", "", "NPC不存在"};
    }

    if (brain->npc_type != NPCType::Ordinary && brain->npc_type != NPCType::HalfAwakened) {
        auto it = s_presets.find(brain->npc_type);
        std::string label = (it != s_presets.end()) ? it->second.type_label : "未知";
        return {false, "complete", brain->npc_type, "", "", "已经觉醒为「" + label + "」"};
    }

    const auto& fate_lock = brain->fate_lock;
    const auto& conflicts = brain->conflicts;
    float self_awareness = brain->selfAwareness;
    float wisdom = brain->wisdom;

    int awakening_attempts = brain->evolution_stage;

    if (brain->npc_type == NPCType::Ordinary && awakening_attempts >= 1) {
        bool should_become_half = (
            self_awareness >= 40.0f &&
            fate_lock.fate_cycle_state != "complacent" &&
            (awakening_attempts >= 2 || wisdom >= 45.0f)
        );

        if (should_become_half) {
            return awaken_to_half_awakened(npc_id);
        }

        return {false, "restless", NPCType::Ordinary, "", "", "已有觉醒迹象，但还未达到半觉醒门槛"};
    }

    if (brain->npc_type == NPCType::HalfAwakened) {
        bool can_metamorphose = (
            awakening_attempts >= 5 &&
            self_awareness >= 70.0f &&
            wisdom >= 60.0f
        );

        if (!can_metamorphose) {
            std::ostringstream msg;
            msg << "觉醒次数(" << awakening_attempts << "/5)或自我意识("
                << (int)self_awareness << "/70)不足";
            return {false, "awakening", NPCType::HalfAwakened, "", "", msg.str()};
        }

        NPCType target = determine_awakening_type(*brain);
        return awaken_to_type(npc_id, target);
    }

    return {false, "dormant", brain->npc_type, "", "", "觉醒条件不满足"};
}

NPCType NPCArchitectureEngine::determine_awakening_type(const NPCBrain& brain) {
    const auto& base_traits = brain.personality.base_traits;
    const auto& values = brain.personality.values;

    auto has_trait = [&](const std::string& keyword) -> bool {
        for (const auto& t : base_traits) {
            if (t.find(keyword) != std::string::npos) return true;
        }
        return false;
    };

    auto has_value = [&](const std::string& keyword) -> bool {
        for (const auto& v : values) {
            if (v.find(keyword) != std::string::npos) return true;
        }
        return false;
    };

    if (brain.wisdom >= 65.0f &&
        brain.selfAwareness >= 60.0f &&
        (has_value("自由") || has_value("意义")))
    {
        return NPCType::Awakened;
    }

    if (brain.fate_lock.comparison_obsession * 100.0f >= 55.0f &&
        brain.dark_side.self_preservation_priority * 100.0f >= 65.0f &&
        (has_trait("理性") || has_trait("冷静") || has_trait("果断")))
    {
        return NPCType::RuthlessRuler;
    }

    if ((has_value("理想") || has_value("信念") || has_value("正义")) &&
        brain.value_system.moral_redlines.size() >= 2 &&
        brain.wisdom >= 50.0f)
    {
        return NPCType::IdealistMartyr;
    }

    if (brain.dark_side.schadenfreude_level >= 0.5f &&
        brain.dark_side.self_preservation_priority >= 0.6f &&
        (has_trait("叛逆") || has_trait("极端") || has_trait("冷漠")))
    {
        return NPCType::Antisocial;
    }

    if (brain.reflections.size() >= 1 &&
        brain.emotion.mood_intensity * 10.0f >= 7.0f &&
        brain.selfAwareness < 50.0f)
    {
        return NPCType::TraumatizedOrdinary;
    }

    if (brain.conflicts.current_mental_drain >= 40.0f &&
        brain.wisdom >= 45.0f)
    {
        return NPCType::IntermittentIdealist;
    }

    return NPCType::EthicalPragmatist;
}

AwakeningResult NPCArchitectureEngine::force_awaken_to_type(
    const std::string& npc_id, NPCType target_type)
{
    return awaken_to_type(npc_id, target_type);
}

AwakeningResult NPCArchitectureEngine::awaken_to_half_awakened(const std::string& npc_id) {
    if (!brain_manager_) {
        return {false, "restless", NPCType::Ordinary, "", "", "NPCBrainManager不可用"};
    }

    NPCBrain* brain = brain_manager_->get_brain(npc_id);
    if (!brain) {
        return {false, "restless", NPCType::Ordinary, "", "", "NPC不存在"};
    }

    brain->npc_type = NPCType::HalfAwakened;
    brain->evolution_stage = std::max(brain->evolution_stage, 1);

    apply_architecture_to_brain(*brain);

    brain_manager_->add_memory(*brain,
        "【觉醒】我隐约觉得自己在按一个固定的人生剧本走，但没有勇气打破它。",
        MemoryType::Core, 0.9f);

    brain_manager_->save_brain(npc_id);

    return {
        true,
        "awakening",
        NPCType::HalfAwakened,
        "half_awakened",
        "半觉醒半沉沦的普通人",
        "觉醒成功！现在处于半觉醒半沉沦状态"
    };
}

AwakeningResult NPCArchitectureEngine::awaken_to_type(
    const std::string& npc_id, NPCType target_type)
{
    if (!brain_manager_) {
        return {false, "metamorphosis", NPCType::HalfAwakened, "", "", "NPCBrainManager不可用"};
    }

    NPCBrain* brain = brain_manager_->get_brain(npc_id);
    if (!brain) {
        return {false, "metamorphosis", NPCType::HalfAwakened, "", "", "NPC不存在"};
    }

    auto it = s_presets.find(target_type);
    if (it == s_presets.end()) {
        return {false, "metamorphosis", NPCType::HalfAwakened, "", "", "目标类型不存在"};
    }

    const auto& arch = it->second;

    brain->npc_type = target_type;

    apply_architecture_to_brain(*brain);

    std::string awakening_memo = "【完全觉醒】我彻底看透了循环，成为了"
        + arch.type_label + "。" + arch.description;
    brain_manager_->add_memory(*brain, awakening_memo, MemoryType::Core, 1.0f);

    ReflectionEntry reflection;
    reflection.id = npc_id + "_refl_" + std::to_string(std::time(nullptr));
    reflection.trigger = "觉醒蜕变";
    reflection.old_belief = "我只是个普通人，按既定的人生轨迹生活";
    reflection.new_insight = "我跳出了循环，成为了" + arch.type_label
        + "。现在我的" + arch.decision_priority[0] + "主导我的决策";
    reflection.timestamp = std::time(nullptr);
    reflection.impact = "major";

    brain->reflections.push_back(std::move(reflection));
    if (brain->reflections.size() > 50) {
        brain->reflections.erase(brain->reflections.begin());
    }

    brain->wisdom        = std::min(100.0f, brain->wisdom + 10.0f);
    brain->selfAwareness  = std::min(100.0f, brain->selfAwareness + 20.0f);
    brain->evolution_stage = std::min(10, brain->evolution_stage + 2);

    brain->fate_lock.fate_cycle_state = "breaking_free";

    brain_manager_->save_brain(npc_id);

    return {
        true,
        "complete",
        target_type,
        NPCTypeToStr(target_type),
        arch.type_label,
        "觉醒成功！现在成为「" + arch.type_label + "」"
    };
}

void NPCArchitectureEngine::apply_architecture_to_brain(NPCBrain& brain) {
    auto it = s_presets.find(brain.npc_type);
    if (it == s_presets.end()) return;
    const auto& custom = it->second;

    if (brain.npc_type == NPCType::Ordinary) return;

    switch (custom.modules.core_needs) {
        case ModuleState::Weakened:
            brain.personality.conscientiousness = std::min(1.0f, brain.personality.conscientiousness * 1.2f);
            brain.needs.hunger *= 0.5f;
            brain.needs.boredom *= 0.5f;
            break;
        case ModuleState::Enhanced:
            brain.personality.conscientiousness = std::min(1.0f, brain.personality.conscientiousness * 0.85f);
            brain.needs.hunger = std::min(1.0f, brain.needs.hunger * 1.3f);
            brain.needs.boredom = std::min(1.0f, brain.needs.boredom * 1.3f);
            break;
        case ModuleState::Disabled:
            brain.needs.hunger = 0.1f;
            brain.needs.fatigue = 0.1f;
            brain.needs.boredom = 0.1f;
            brain.needs.loneliness = 0.1f;
            brain.needs.stress = 0.1f;
            break;
        default: break;
    }

    switch (custom.modules.emotion_system) {
        case ModuleState::Weakened:
            brain.emotion.mood_intensity = std::max(0.1f, brain.emotion.mood_intensity * 0.5f);
            brain.emotion.stress_level   = std::max(0.1f, brain.emotion.stress_level * 0.6f);
            break;
        case ModuleState::Enhanced:
            brain.emotion.mood_intensity = std::min(1.0f, brain.emotion.mood_intensity * 1.3f);
            brain.emotion.happiness_level = std::min(1.0f, brain.emotion.happiness_level * 0.8f);
            break;
        case ModuleState::Disabled:
            brain.emotion.current_mood = "平静";
            brain.emotion.mood_intensity = 0.1f;
            brain.emotion.happiness_level = 0.5f;
            brain.emotion.stress_level = 0.0f;
            break;
        default: break;
    }

    if (custom.modules.persona_performance == ModuleState::Disabled) {
        brain.identity_lock.assertions.clear();
        brain.identity_lock.immutable_facts.clear();
    }

    switch (custom.modules.internal_conflict) {
        case ModuleState::Disabled:
            brain.conflicts.core_conflict_pairs.clear();
            brain.conflicts.current_mental_drain = 0.0f;
            break;
        case ModuleState::Enhanced:
            brain.conflicts.current_mental_drain = std::min(100.0f,
                brain.conflicts.current_mental_drain + 30.0f);
            break;
        default: break;
    }

    if (custom.modules.subconscious_defense == ModuleState::Disabled) {
        brain.subconscious.repressed_contents.clear();
        brain.subconscious.daydream_frequency = 0.0f;
    }

    switch (custom.modules.dark_side) {
        case ModuleState::Disabled:
            brain.dark_side.schadenfreude_level = 0.0f;
            brain.dark_side.self_preservation_priority = 0.1f;
            brain.dark_side.self_leniency = 0.1f;
            brain.dark_side.others_strictness = 0.1f;
            break;
        case ModuleState::Enhanced:
            brain.dark_side.schadenfreude_level = std::min(1.0f,
                brain.dark_side.schadenfreude_level + 0.3f);
            brain.dark_side.self_leniency = std::min(1.0f,
                brain.dark_side.self_leniency + 0.2f);
            brain.dark_side.others_strictness = std::min(1.0f,
                brain.dark_side.others_strictness + 0.15f);
            brain.dark_side.self_preservation_priority = std::min(1.0f,
                brain.dark_side.self_preservation_priority + 0.15f);
            break;
        default: break;
    }

    switch (custom.modules.social_interaction) {
        case ModuleState::Disabled:
            brain.fate_lock.comparison_obsession = 0.0f;
            brain.fate_lock.fear_of_standing_out = 0.0f;
            break;
        case ModuleState::Enhanced:
            brain.fate_lock.comparison_obsession = std::min(1.0f,
                brain.fate_lock.comparison_obsession + 0.25f);
            brain.fate_lock.fear_of_standing_out = std::min(1.0f,
                brain.fate_lock.fear_of_standing_out + 0.1f);
            break;
        default: break;
    }

    switch (custom.modules.self_cognition) {
        case ModuleState::Enhanced:
            brain.selfAwareness = std::min(100.0f, brain.selfAwareness + 20.0f);
            break;
        case ModuleState::Weakened:
            brain.selfAwareness = std::max(10.0f, brain.selfAwareness - 15.0f);
            break;
        default: break;
    }

    if (custom.modules.memory_system == ModuleState::Disabled) {
        brain.long_term_memory.clear();
        brain.core_memory.clear();
        brain.reflections.clear();
    } else if (custom.modules.memory_system == ModuleState::Enhanced) {
        brain.conflicts.current_mental_drain = std::min(100.0f,
            brain.conflicts.current_mental_drain + 10.0f);
    }

    if (custom.fate_lock_strength <= 0.0f) {
        brain.fate_lock.busyness_level = 0.05f;
        brain.fate_lock.comparison_obsession = 0.0f;
        brain.fate_lock.fear_of_standing_out = 0.0f;
        brain.fate_lock.fate_cycle_state = "aware_but_trapped";
    } else if (custom.fate_lock_strength >= 70.0f) {
        brain.fate_lock.fate_cycle_state = "complacent";
    }

    if (custom.modules.body_instinct == ModuleState::Disabled) {
        brain.body.stamina = 1.0f;
        brain.body.pain_threshold = 1.0f;
    } else if (custom.modules.body_instinct == ModuleState::Enhanced) {
        brain.body.stamina = std::min(1.0f, brain.body.stamina * 1.3f);
    }
}

static const std::unordered_map<NPCType, std::string> s_type_str_map = {
    {NPCType::Ordinary,             "ordinary"},
    {NPCType::Awakened,             "awakened"},
    {NPCType::RuthlessRuler,        "ruthless_ruler"},
    {NPCType::IdealistMartyr,       "idealist_martyr"},
    {NPCType::Antisocial,           "antisocial"},
    {NPCType::Pathological,         "pathological"},
    {NPCType::FateDominated,        "fate_dominated"},
    {NPCType::HalfAwakened,         "half_awakened"},
    {NPCType::EthicalPragmatist,    "ethical_pragmatist"},
    {NPCType::IntermittentIdealist, "intermittent_idealist"},
    {NPCType::TraumatizedOrdinary,  "traumatized_ordinary"},
    {NPCType::Divine,               "divine"},
    {NPCType::Bestial,              "bestial"},
};

std::string NPCTypeToStr(NPCType type) {
    auto it = s_type_str_map.find(type);
    return (it != s_type_str_map.end()) ? it->second : "ordinary";
}