#include "npc_local_response.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

bool LocalResponseEngine::contains_any(const std::string& text,
                                        const std::vector<std::string>& keywords) {
    for (const auto& kw : keywords) {
        if (text.find(kw) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string LocalResponseEngine::trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && static_cast<unsigned char>(*start) <= 0x20) {
        ++start;
    }
    auto end = s.end();
    while (end != start && static_cast<unsigned char>(*(end - 1)) <= 0x20) {
        --end;
    }
    return std::string(start, end);
}

std::string LocalResponseEngine::detect_scenario(const NPCBrain& brain,
                                                   const std::string& input) {
    const auto trimmed = trim(input);

    {
        static const std::vector<std::string> keywords = {
            "你是谁", "你叫什么", "你是什么人", "你叫什么名字",
            "怎么称呼", "你的名字"
        };
        if (contains_any(trimmed, keywords)) {
            return "identity";
        }
    }

    {
        static const std::vector<std::string> keywords = {
            "你好", "嗨", "hello", "hi", "您好",
            "早上好", "下午好", "晚上好", "哈喽",
            "在吗", "来了"
        };
        if (contains_any(trimmed, keywords)) {
            return "greeting";
        }
    }

    {
        static const std::vector<std::string> keywords = {
            "天气", "下雨", "出太阳", "好冷", "好热",
            "下雪", "刮风", "温度", "降温", "升温"
        };
        if (contains_any(trimmed, keywords) && trimmed.length() < 50) {
            return "weather";
        }
    }

    {
        static const std::vector<std::string> keywords = {
            "是的", "你说的对", "没错", "嗯", "对",
            "有道理", "确实", "说的对", "同意", "赞同",
            "就是这样"
        };
        if (contains_any(trimmed, keywords)) {
            return "agreement";
        }
    }

    {
        static const std::vector<std::string> keywords = {
            "记得", "还记得", "想起来", "以前", "上次",
            "过去", "曾经", "发生过", "那时候"
        };
        if (contains_any(trimmed, keywords)) {
            const bool has_memories =
                !brain.long_term_memory.empty() ||
                !brain.short_term_memory.empty();
            if (has_memories) {
                return "memory_recall";
            }
        }
    }

    {
        static const std::vector<std::string> keywords = {
            "再见", "拜拜", "下次", "回头见", "走了",
            "告辞", "晚安", "明天见", "先走了", "回见"
        };
        if (contains_any(trimmed, keywords)) {
            return "farewell";
        }
    }

    if (!brain.knowledge.empty()) {
        static const std::vector<std::string> keywords = {
            "是什么", "在哪", "为什么", "怎么样",
            "多少", "哪个", "谁", "知道", "认识", "了解"
        };
        if (contains_any(trimmed, keywords)) {
            return "knowledge_query";
        }
    }

    return "";
}

std::string LocalResponseEngine::respond_identity(const NPCBrain& brain) {
    const auto& name = brain.personality.name;
    const auto& role = brain.personality.role;

    static const char* templates[] = {
        "%s，%s",
        "我是%s",
        "%s，很高兴认识你",
        "哦，我是%s，%s"
    };
    const int idx = std::rand() % 4;
    char buf[256];
    if (idx == 0 || idx == 3) {
        std::snprintf(buf, sizeof(buf), templates[idx], name.c_str(), role.c_str());
    } else if (idx == 1) {
        std::snprintf(buf, sizeof(buf), templates[idx], name.c_str());
    } else {
        std::snprintf(buf, sizeof(buf), templates[idx], name.c_str());
    }
    return buf;
}

std::string LocalResponseEngine::respond_greeting(const NPCBrain& brain) {
    float max_affection = 0.0f;
    for (const auto& [id, rel] : brain.relationships) {
        if (rel.affection > max_affection) {
            max_affection = rel.affection;
        }
    }

    const float happiness = brain.emotion.happiness_level;
    const float energy = brain.mental_energy;
    const float warmth = (max_affection + happiness * 100.0f) / 2.0f;

    if (energy < 0.35f) {
        static const char* low_energy[] = {
            "嗯...你好",
            "嗨...",
            "你好啊，有点困",
            "嗯，我在。"
        };
        return low_energy[std::rand() % 4];
    }

    if (warmth > 60.0f) {
        static const char* warm[] = {
            "你来啦！",
            "嘿，正想你呢",
            "太好了，见到你"
        };
        return warm[std::rand() % 3];
    }

    if (warmth < 20.0f) {
        static const char* cold[] = {
            "你好。",
            "嗯。",
            "有什么事吗？"
        };
        return cold[std::rand() % 3];
    }

    static const char* neutral[] = {
        "你好",
        "嗨，你好",
        "你好呀"
    };
    return neutral[std::rand() % 3];
}

std::string LocalResponseEngine::respond_weather(const NPCBrain& brain) {
    const float valence = brain.emotion.background_mood.valence;

    if (valence > 0.3f) {
        static const char* positive[] = {
            "是啊，今天天气真不错",
            "嗯，这样的天气让人心情好",
            "确实，很适合出去走走"
        };
        return positive[std::rand() % 3];
    }

    if (valence < -0.3f) {
        static const char* negative[] = {
            "嗯，这天气真是...",
            "是啊，让人有点提不起劲",
            "唉，希望快点好转"
        };
        return negative[std::rand() % 3];
    }

    static const char* neutral[] = {
        "嗯，还行吧",
        "是啊",
        "确实如此"
    };
    return neutral[std::rand() % 3];
}

std::string LocalResponseEngine::respond_agreement() {
    static const char* phrases[] = {
        "没错",
        "确实如此",
        "是啊",
        "嗯，有道理",
        "说的对",
        "对的"
    };
    return phrases[std::rand() % 6];
}

std::string LocalResponseEngine::respond_memory_recall(const NPCBrain& brain,
                                                        const std::string& input) {
    const auto& memories = brain.long_term_memory;
    if (memories.empty()) {
        if (brain.short_term_memory.empty()) {
            return "让我想想...好像记不太清了";
        }
        return respond_memory_recall_from(brain.short_term_memory, input);
    }
    return respond_memory_recall_from(memories, input);
}

std::string LocalResponseEngine::respond_memory_recall_from(
        const std::vector<BrainMemory>& memories,
        const std::string& input) {
    std::vector<std::pair<float, const BrainMemory*>> scored;
    for (const auto& mem : memories) {
        if (mem.content.length() < 5) continue;
        float score = 0.0f;
        std::istringstream iss(input);
        std::string word;
        while (iss >> word) {
            if (mem.content.find(word) != std::string::npos) {
                score += 1.0f;
            }
        }
        score += mem.importance * 0.5f;
        if (score > 0.0f) {
            scored.emplace_back(score, &mem);
        }
    }

    if (scored.empty()) {
        return "让我想想...好像记不太清了";
    }

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    const auto& best = *scored[0].second;
    const std::string snippet = best.content.substr(0, 60);

    static const char* templates[] = {
        "我记得，%s",
        "是有这么回事...%s",
        "嗯，想起来就觉得%s"
    };
    char buf[256];
    const int idx = std::rand() % 3;
    if (idx == 2) {
        const char* feeling = best.emotional_valence > 0.0f ? "挺温暖的" : "不太好受";
        std::snprintf(buf, sizeof(buf), templates[idx], feeling);
    } else {
        std::snprintf(buf, sizeof(buf), templates[idx], snippet.c_str());
    }
    return buf;
}

std::string LocalResponseEngine::respond_farewell(const NPCBrain& brain) {
    float max_affection = 0.0f;
    for (const auto& [id, rel] : brain.relationships) {
        if (rel.affection > max_affection) {
            max_affection = rel.affection;
        }
    }

    if (max_affection > 60.0f) {
        static const char* warm[] = {
            "嗯，保重",
            "好的，下次见",
            "路上小心"
        };
        return warm[std::rand() % 3];
    }

    static const char* default_farewells[] = {
        "嗯，再见",
        "好",
        "行，回见"
    };
    return default_farewells[std::rand() % 3];
}

std::string LocalResponseEngine::respond_knowledge_query(const NPCBrain& brain,
                                                          const std::string& input) {
    std::vector<const KnowledgeEntry*> valid;
    for (const auto& k : brain.knowledge) {
        if (k.confidence > 30.0f) {
            valid.push_back(&k);
        }
    }

    if (valid.empty()) {
        return "这个我不太清楚";
    }

    std::sort(valid.begin(), valid.end(),
              [](const KnowledgeEntry* a, const KnowledgeEntry* b) {
                  return a->confidence > b->confidence;
              });

    const KnowledgeEntry* exact_match = nullptr;
    for (const auto* k : valid) {
        std::string query = input;
        for (const auto& stop : {"吗", "呢", "吧", "的", "了", "么", "啊"}) {
            size_t pos;
            while ((pos = query.find(stop)) != std::string::npos) {
                query.erase(pos, 1);
            }
        }
        bool matched = false;
        for (size_t i = 0; i < query.length() && !matched; ++i) {
            if (k->content.find(query[i]) != std::string::npos) {
                matched = true;
            }
        }
        if (matched) {
            exact_match = k;
            break;
        }
    }

    const KnowledgeEntry& answer = exact_match ? *exact_match : *valid[0];
    char buf[512];

    if (answer.confidence > 70.0f) {
        static const char* high_conf[] = {
            "据我所知，%s",
            "我记得，%s",
            "我确定，%s"
        };
        std::snprintf(buf, sizeof(buf),
                      high_conf[std::rand() % 3], answer.content.c_str());
    } else {
        static const char* low_conf[] = {
            "我好像记得，%s",
            "如果没记错的话，%s",
            "好像是%s"
        };
        std::snprintf(buf, sizeof(buf),
                      low_conf[std::rand() % 3], answer.content.c_str());
    }

    return buf;
}

LocalResponseResult LocalResponseEngine::try_local_response(
        const NPCBrain& brain, const std::string& input) {
    stats_.total_attempts++;

    const std::string scenario = detect_scenario(brain, input);
    if (scenario.empty()) {
        return LocalResponseResult{};
    }

    stats_.local_hits++;

    LocalResponseResult result;
    result.handled = true;
    result.response_type = scenario;

    if (scenario == "identity") {
        result.response = respond_identity(brain);
    } else if (scenario == "greeting") {
        result.response = respond_greeting(brain);
    } else if (scenario == "weather") {
        result.response = respond_weather(brain);
    } else if (scenario == "agreement") {
        result.response = respond_agreement();
    } else if (scenario == "memory_recall") {
        result.response = respond_memory_recall(brain, input);
    } else if (scenario == "farewell") {
        result.response = respond_farewell(brain);
    } else if (scenario == "knowledge_query") {
        result.response = respond_knowledge_query(brain, input);
    }

    return result;
}