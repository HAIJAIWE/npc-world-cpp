#include "npc_learning.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <set>
#include <sstream>
#include <unordered_set>
#include <vector>

const std::unordered_map<std::string, std::vector<std::string>> NPCLearningEngine::DOMAIN_KEYWORDS = {
    {"combat",   {"战斗", "打", "杀", "攻击", "防御"}},
    {"social",   {"社交", "朋友", "关系", "信任"}},
    {"craft",    {"制作", "工具", "手艺", "建造"}},
    {"scholar",  {"知识", "学习", "读书", "历史"}},
    {"percept",  {"观察", "感知", "察觉", "发现"}},
    {"will",     {"意志", "决心", "忍耐", "坚持"}},
};

static const std::unordered_set<std::string> STOP_WORDS = {
    "的", "了", "是", "在", "我", "有", "和", "就", "不", "人", "都", "一",
    "一个", "上", "也", "很", "到", "说", "要", "去", "你", "会", "着",
    "没有", "看", "好", "自己", "这", "他", "她", "它", "们", "那", "被",
    "从", "而", "与", "但", "或", "如果", "因为", "所以", "可以", "这个",
    "那个", "什么", "怎么", "为什么", "能", "让", "把", "给", "用", "对",
    "向", "当", "然后", "之后", "之前", "已经", "还", "更", "最", "非常",
    "做", "想", "知道", "觉得", "应该", "可能", "会", "才能", "只是",
};

std::string NPCLearningEngine::infer_domain(const std::string& content) {
    for (const auto& [domain, keywords] : DOMAIN_KEYWORDS) {
        for (const auto& kw : keywords) {
            if (content.find(kw) != std::string::npos) {
                return domain;
            }
        }
    }
    return "scholar";
}

std::vector<std::string> NPCLearningEngine::extract_keywords(const std::string& text) {
    std::string cleaned = text;
    const std::string punct = "，。！？、；：""''（）";
    for (auto& ch : cleaned) {
        if (punct.find(ch) != std::string::npos) {
            ch = ' ';
        }
    }

    std::unordered_set<std::string> seen;
    std::vector<std::string> result;
    std::istringstream stream(cleaned);
    std::string word;
    while (stream >> word) {
        if (word.size() >= 2 && STOP_WORDS.find(word) == STOP_WORDS.end() && seen.find(word) == seen.end()) {
            seen.insert(word);
            result.push_back(word);
        }
    }
    return result;
}

float NPCLearningEngine::keyword_similarity(const std::string& a, const std::string& b) {
    auto ka = extract_keywords(a);
    auto kb = extract_keywords(b);

    std::unordered_set<std::string> set_a(ka.begin(), ka.end());
    std::unordered_set<std::string> set_b(kb.begin(), kb.end());

    if (set_a.empty() && set_b.empty()) return 0.0f;

    size_t intersection = 0;
    for (const auto& w : set_a) {
        if (set_b.count(w)) ++intersection;
    }

    size_t union_size = set_a.size() + set_b.size() - intersection;
    return union_size == 0 ? 0.0f : static_cast<float>(intersection) / static_cast<float>(union_size);
}

float NPCLearningEngine::get_source_weight(const std::string& source) {
    if (source == "self")           return 60.0f;
    if (source == "observed")       return 50.0f;
    if (source == "overheard")      return 30.0f;
    return 15.0f;
}

DissonanceResult NPCLearningEngine::process_new_knowledge(
    NPCBrain& brain,
    const std::string& domain,
    const std::string& content,
    const std::string& source,
    float confidence)
{
    DissonanceResult result;
    result.has_dissonance = false;

    const KnowledgeEntry* best_conflict = nullptr;
    float best_similarity = 0.0f;

    for (const auto& existing : brain.knowledge) {
        if (existing.content == content) continue;
        if (existing.domain != domain) continue;

        float sim = keyword_similarity(content, existing.content);
        if (sim > 0.4f && sim > best_similarity) {
            best_similarity = sim;
            best_conflict = &existing;
        }
    }

    if (!best_conflict) {
        result.has_dissonance = false;
        return result;
    }

    float new_base = get_source_weight(source);
    float new_conf = new_base + confidence;

    float old_conf = best_conflict->confidence;

    std::string resolution = rule_adjudicate(brain, new_conf, old_conf, old_conf);

    result.has_dissonance = true;
    result.resolution = resolution;

    if (resolution == "accepted_new") {
        result.narrative = "原来我一直以为\"" +
            best_conflict->content.substr(0, 20) + "\"，现在意识到可能是错的。";
    } else if (resolution == "rejected_new") {
        result.narrative = "\"" + content.substr(0, 20) + "\"听起来有道理，但我还是更相信自己的判断。";
    } else if (resolution == "synthesized") {
        result.narrative = "\"" + best_conflict->content.substr(0, 15) + "\"和\"" +
            content.substr(0, 15) + "\"各有道理，我想我能找到共同点。";
    } else {
        result.narrative = "\"" + best_conflict->content.substr(0, 15) + "\"和\"" +
            content.substr(0, 15) + "\"两个说法我先都记着吧。";
    }

    CognitiveDissonance dissonance;
    dissonance.id = std::to_string(brain.learning_episodes + 1) + "_" + std::to_string(brain.total_interactions);
    dissonance.new_knowledge = content;
    dissonance.conflict_content = best_conflict->content;
    dissonance.conflict_confidence = best_conflict->confidence;
    dissonance.resolution = resolution;
    dissonance.narrative = result.narrative;
    dissonance.triggered_at = static_cast<int64_t>(std::time(nullptr));
    brain.cognitive_dissonances.push_back(std::move(dissonance));

    brain.selfAwareness = std::min(100.0f, brain.selfAwareness + 3.0f);
    brain.wisdom = std::min(100.0f, brain.wisdom + 1.0f);

    ReflectionEntry reflection;
    reflection.id = "refl_" + std::to_string(brain.learning_episodes);
    reflection.trigger = "认知冲突";
    reflection.new_insight = result.narrative;
    reflection.impact = "major";
    reflection.timestamp = static_cast<int64_t>(std::time(nullptr));
    brain.reflections.push_back(std::move(reflection));

    brain.learning_episodes++;

    return result;
}

ObservationalResult NPCLearningEngine::process_social_observation(
    NPCBrain& brain,
    const SocialObservation& observation)
{
    ObservationalResult result;

    float source_weight = 1.0f;
    if (observation.source == "overheard")  source_weight = 0.6f;
    else if (observation.source == "gossip") source_weight = 0.3f;

    float confidence = observation.trust * source_weight;

    float conformity = brain.fate_lock.fear_of_standing_out;
    if (conformity > 60.0f) confidence -= 15.0f;
    if (conformity > 80.0f) confidence -= 30.0f;

    float status_anxiety = brain.fate_lock.comparison_obsession;
    if (status_anxiety > 50.0f) confidence *= 0.5f;

    bool is_awakened = (brain.npc_type != NPCType::Ordinary &&
                        brain.npc_type != NPCType::HalfAwakened);
    if (is_awakened) {
        confidence = std::max(confidence, observation.trust * 0.7f);
    }

    confidence = std::max(5.0f, std::min(100.0f, confidence));

    ObservationalLesson lesson;
    lesson.id = "obs_" + std::to_string(brain.learning_episodes) + "_" + std::to_string(brain.total_interactions);
    lesson.source = observation.source;
    lesson.source_npc_id = observation.source_npc_id;
    lesson.cause = observation.cause;
    lesson.effect = observation.effect;
    lesson.valence = observation.valence;
    lesson.confidence = confidence;
    lesson.domain = infer_domain(observation.cause + " " + observation.effect);
    lesson.stored_at = static_cast<int64_t>(std::time(nullptr));

    brain.observational_lessons.push_back(std::move(lesson));

    result.learned = true;
    result.confidence = confidence;

    return result;
}

float NPCLearningEngine::get_behavior_influence(
    const NPCBrain& brain,
    const std::string& domain)
{
    std::vector<const ObservationalLesson*> relevant;
    for (const auto& lesson : brain.observational_lessons) {
        if (lesson.domain == domain) {
            relevant.push_back(&lesson);
        }
    }

    if (relevant.empty()) return 0.0f;

    float sum = 0.0f;
    for (const auto* l : relevant) {
        float sign = (l->valence == "positive") ? 1.0f : -1.0f;
        sum += sign * l->confidence;
    }
    float avg = sum / static_cast<float>(relevant.size());

    return 1.0f + avg * 0.002f;
}

std::string NPCLearningEngine::rule_adjudicate(
    const NPCBrain& brain,
    float new_confidence,
    float old_confidence,
    float old_confidence_value)
{
    float openness_bonus = 0.0f;
    float stubborn_bonus = 0.0f;

    if (brain.personality.openness >= 0.6f) {
        openness_bonus = 30.0f;
    }

    for (const auto& trait : brain.personality.base_traits) {
        if (trait.find("开放") != std::string::npos ||
            trait.find("包容") != std::string::npos) {
            openness_bonus = std::max(openness_bonus, 30.0f);
        }
        if (trait.find("固执") != std::string::npos ||
            trait.find("倔") != std::string::npos) {
            stubborn_bonus = std::max(stubborn_bonus, 50.0f);
        }
    }

    if (brain.personality.conscientiousness < 0.3f) {
        stubborn_bonus = std::max(stubborn_bonus, 20.0f);
    }

    float effective_new = new_confidence + openness_bonus;
    float effective_old = old_confidence + stubborn_bonus;

    if (brain.wisdom >= 60.0f) {
        float diff = std::abs(effective_new - effective_old);
        if (diff < 25.0f) {
            return "synthesized";
        }
    }

    if (effective_new > effective_old + 15.0f) {
        return "accepted_new";
    }

    if (effective_old > effective_new + 25.0f) {
        return "rejected_new";
    }

    return "coexist";
}