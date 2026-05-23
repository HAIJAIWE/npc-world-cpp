#include "npc_self_improve.h"
#include <algorithm>
#include <chrono>
#include <sstream>

NPCImprovementEngine::NPCImprovementEngine() {}

std::string NPCImprovementEngine::generate_id(const std::string& prefix) {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::ostringstream oss;
    oss << prefix << "_" << now;
    return oss.str();
}

bool NPCImprovementEngine::detect_impulsive_pattern(const NPCBrain& brain, int& out_conflicts, int& out_regrets) {
    out_conflicts = 0;
    out_regrets = 0;

    for (const auto& mem : brain.short_term_memory) {
        if (mem.content.find("吵") != std::string::npos ||
            mem.content.find("争论") != std::string::npos ||
            mem.content.find("冲突") != std::string::npos ||
            mem.content.find("生气") != std::string::npos ||
            mem.content.find("难过") != std::string::npos) {
            out_conflicts++;
        }
    }

    out_regrets = static_cast<int>(brain.self_concept.regrets.size());

    bool has_anger_now =
        (brain.emotion.current_mood == "愤怒" && brain.emotion.mood_intensity >= 6.0f) ||
        (brain.emotion.immediate_emotion.type == "愤怒" && brain.emotion.immediate_emotion.intensity >= 6.0f);

    return out_conflicts >= 2 && (out_regrets >= 2 || has_anger_now);
}

bool NPCImprovementEngine::detect_social_exhaustion(const NPCBrain& brain) {
    float maintenance_effort = (1.0f - brain.mental_energy) * 100.0f;
    return maintenance_effort > 65.0f;
}

bool NPCImprovementEngine::detect_avoidance_pattern(const NPCBrain& brain, int& out_count) {
    out_count = 0;
    for (const auto& mem : brain.short_term_memory) {
        if (mem.content.find("逃避") != std::string::npos ||
            mem.content.find("拖延") != std::string::npos ||
            mem.content.find("回避") != std::string::npos) {
            out_count++;
        }
    }
    return out_count >= 3;
}

std::vector<ImprovementIntent> NPCImprovementEngine::detect_patterns(const NPCBrain& brain) {
    auto& existing = intents_by_npc_[brain.npc_id];

    auto has_active_intent = [&](const std::string& type) -> bool {
        return std::any_of(existing.begin(), existing.end(),
            [&](const ImprovementIntent& i) {
                return i.pattern_type == type && i.active;
            });
    };

    std::vector<ImprovementIntent> detected;

    if (!has_active_intent("impulsive")) {
        int conflicts = 0, regrets = 0;
        if (detect_impulsive_pattern(brain, conflicts, regrets)) {
            int observed = std::max(conflicts, regrets);
            auto intent = form_improvement_intent(brain, "impulsive", observed);
            if (intent.active) {
                detected.push_back(intent);
            }
        }
    }

    if (!has_active_intent("social_exhaustion")) {
        if (detect_social_exhaustion(brain)) {
            auto intent = form_improvement_intent(brain, "social_exhaustion", 3);
            if (intent.active) {
                detected.push_back(intent);
            }
        }
    }

    if (!has_active_intent("avoidance")) {
        int avoid_count = 0;
        if (detect_avoidance_pattern(brain, avoid_count)) {
            auto intent = form_improvement_intent(brain, "avoidance", avoid_count);
            if (intent.active) {
                detected.push_back(intent);
            }
        }
    }

    return detected;
}

ImprovementIntent NPCImprovementEngine::form_improvement_intent(
    const NPCBrain& brain,
    const std::string& pattern_type,
    int times_observed)
{
    ImprovementIntent intent;
    intent.active = false;

    if (brain.selfAwareness < 30.0f) {
        return intent;
    }

    intent.id = generate_id(pattern_type);
    intent.pattern_type = pattern_type;
    intent.times_observed = times_observed;
    intent.created_at = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    intent.last_evaluated = intent.created_at;
    intent.confidence = 20.0f + static_cast<float>(times_observed) * 5.0f;
    intent.active = true;

    if (pattern_type == "impulsive") {
        intent.goal = "在被批评时先深呼吸再回应";
        intent.motivation = "减少后悔";
        intent.description = "冲动反应：情绪一上来就怼回去，事后又后悔";
    } else if (pattern_type == "social_exhaustion") {
        intent.goal = "每天给自己独处时间";
        intent.motivation = "恢复社交能量";
        intent.description = "社交耗尽：强撑笑脸，内心疲惫";
    } else if (pattern_type == "avoidance") {
        intent.goal = "遇到困难先尝试5分钟";
        intent.motivation = "克服拖延";
        intent.description = "回避逃避：面对困难任务时找借口拖延";
    }

    auto& existing = intents_by_npc_[brain.npc_id];
    existing.push_back(intent);

    return intent;
}

void NPCImprovementEngine::evaluate_outcome(
    NPCBrain& brain,
    const std::string& npc_response,
    const std::string& context)
{
    auto& intents = intents_by_npc_[brain.npc_id];
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    bool has_regret = npc_response.find("后悔") != std::string::npos ||
                      npc_response.find("不该") != std::string::npos ||
                      npc_response.find("说错") != std::string::npos;

    bool mood_worsened = brain.emotion.stress_level > 0.6f ||
        ((brain.emotion.current_mood == "愤怒" ||
          brain.emotion.current_mood == "悲伤" ||
          brain.emotion.current_mood == "恐惧") &&
         brain.emotion.mood_intensity >= 7.0f);

    bool outcome_bad = has_regret || mood_worsened;

    for (auto& intent : intents) {
        if (!intent.active) continue;

        intent.attempts++;
        intent.last_evaluated = now;

        if (outcome_bad) {
            intent.failures++;
            intent.confidence = std::max(10.0f, intent.confidence - 5.0f);
        } else {
            intent.successes++;
            intent.confidence = std::min(100.0f, intent.confidence + 3.0f);
        }

        update_habit(brain.npc_id, intent.id, !outcome_bad);

        if (intent.attempts >= 5 &&
            static_cast<float>(intent.successes) / static_cast<float>(intent.attempts) >= 0.8f)
        {
            intent.active = false;

            ReflectionEntry reflection;
            reflection.id = generate_id("habit_formed");
            reflection.trigger = "习惯养成";
            reflection.new_insight = "我终于改掉了「" + intent.pattern_type + "」的毛病。" +
                                     intent.goal + "现在是我的新习惯了。";
            reflection.timestamp = now;
            reflection.impact = "major";
            brain.reflections.push_back(reflection);
        }

        if (intent.failures >= 3 && intent.successes == 0) {
            intent.confidence = 10.0f;
            intent.active = false;

            ReflectionEntry reflection;
            reflection.id = generate_id("self_doubt");
            reflection.trigger = "自我怀疑";
            reflection.new_insight = "我试过改变「" + intent.pattern_type + "」，但太难了。也许我就是这样的人吧。";
            reflection.timestamp = now;
            reflection.impact = "major";
            brain.reflections.push_back(reflection);
        }
    }
}

const std::vector<ImprovementIntent>& NPCImprovementEngine::get_intents(const std::string& npc_id) const {
    static const std::vector<ImprovementIntent> empty;
    auto it = intents_by_npc_.find(npc_id);
    if (it != intents_by_npc_.end()) {
        return it->second;
    }
    return empty;
}

void NPCImprovementEngine::update_habit(
    const std::string& npc_id,
    const std::string& intent_id,
    bool success)
{
    if (!success) return;

    auto& habits = habits_by_npc_[npc_id];
    auto it = std::find_if(habits.begin(), habits.end(),
        [&](const HabitTracker& h) { return h.intent_id == intent_id; });

    HabitTracker* habit = nullptr;
    if (it == habits.end()) {
        HabitTracker h;
        h.intent_id = intent_id;
        h.habit_name = intent_id.substr(0, 20);
        h.consecutive_days = 0;
        h.stage = "struggling";
        habits.push_back(h);
        habit = &habits.back();
    } else {
        habit = &(*it);
    }

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto& intents = intents_by_npc_[npc_id];
    auto intent_it = std::find_if(intents.begin(), intents.end(),
        [&](const ImprovementIntent& i) { return i.id == intent_id; });

    if (intent_it != intents.end()) {
        int64_t time_diff = now - intent_it->last_evaluated;
        if (time_diff < 172800000LL && time_diff >= 0) {
            habit->consecutive_days++;
        } else if (intent_it->last_evaluated < now - 86400000LL * 30) {
            habit->consecutive_days = 1;
        } else {
            habit->consecutive_days = std::max(1, habit->consecutive_days + 1);
        }
    } else {
        habit->consecutive_days++;
    }

    habit->stage = get_habit_stage(habit->consecutive_days);
}

std::string NPCImprovementEngine::get_habit_stage(int consecutive_days) {
    if (consecutive_days >= 30) {
        return "established";
    } else if (consecutive_days >= 14) {
        return "forming";
    } else if (consecutive_days >= 3) {
        return "trying";
    }
    return "struggling";
}