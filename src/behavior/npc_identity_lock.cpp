#include "npc_identity_lock.h"
#include "npc_brain.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <sstream>
#include <random>
#include <unordered_set>

namespace {

std::string escape_regex_impl(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '.': case '*': case '+': case '?': case '^':
            case '$': case '{': case '}': case '(': case ')':
            case '|': case '[': case ']': case '\\':
                out.push_back('\\');
                out.push_back(c);
                break;
            default:
                out.push_back(c);
        }
    }
    return out;
}

const std::vector<std::regex> TONE_AGGRESSIVE = {
    std::regex(R"(你他[妈马])"),
    std::regex(R"(找死)"),
    std::regex(R"(滚开)"),
    std::regex(R"(闭嘴)"),
    std::regex(R"(废[话物])"),
    std::regex(R"(弄死)"),
    std::regex(R"(杀了你)"),
    std::regex(R"(混蛋)"),
    std::regex(R"(王八蛋)"),
    std::regex(R"(揍你)"),
    std::regex(R"(打你)"),
    std::regex(R"(去死)"),
    std::regex(R"(nmsl)", std::regex::icase),
    std::regex(R"(sb)", std::regex::icase),
};

const std::vector<std::regex> TONE_SARCASTIC = {
    std::regex(R"(呵呵.*真是)"),
    std::regex(R"(好棒棒)"),
    std::regex(R"(真了不起)"),
    std::regex(R"(哦.*是吗)"),
    std::regex(R"(就这)"),
    std::regex(R"(不然呢)"),
    std::regex(R"(你说得对.*[但可])"),
    std::regex(R"(真是厉害)"),
    std::regex(R"(不愧是你)"),
};

const std::vector<std::regex> TONE_SUBMISSIVE = {
    std::regex(R"(求求你)"),
    std::regex(R"(饶了我)"),
    std::regex(R"(我错了.*原谅)"),
    std::regex(R"(都是我不好)"),
    std::regex(R"(我配不上)"),
    std::regex(R"(我不配)"),
    std::regex(R"(全都听你的)"),
    std::regex(R"(任凭.*处置)"),
    std::regex(R"(求你.*别)"),
};

const std::vector<std::regex> TONE_CHEERFUL = {
    std::regex(R"(太棒了)"),
    std::regex(R"(好开心)"),
    std::regex(R"(好高兴)"),
    std::regex(R"(真有趣)"),
    std::regex(R"(哈哈.*真)"),
    std::regex(R"(好期待)"),
    std::regex(R"(太好了)"),
    std::regex(R"(最喜欢)"),
    std::regex(R"(超级.*好)"),
    std::regex(R"(开心.*了)"),
};

const std::vector<std::regex> TONE_MELANCHOLIC = {
    std::regex(R"(唉.*[命活])"),
    std::regex(R"(人生.*无[聊奈])"),
    std::regex(R"(一切都.*没意义)"),
    std::regex(R"(好累)"),
    std::regex(R"(没意思)"),
    std::regex(R"(空荡荡)"),
    std::regex(R"(好寂寞)"),
    std::regex(R"(孤独.*死)"),
    std::regex(R"(世界.*灰暗)"),
};

const std::vector<std::regex> TONE_COLD = {
    std::regex(R"(关我什么事)"),
    std::regex(R"(无所谓)"),
    std::regex(R"(随便你怎么想)"),
    std::regex(R"(跟我没关系)"),
    std::regex(R"(你爱怎么)") ,
    std::regex(R"(懒得理)"),
    std::regex(R"(不想管)"),
    std::regex(R"(与我无关)"),
};

const std::vector<std::regex> TONE_PASSIONATE = {
    std::regex(R"(我发誓)"),
    std::regex(R"(我一定会)"),
    std::regex(R"(永远)"),
    std::regex(R"(绝对不)"),
    std::regex(R"(无论如何.*都)"),
    std::regex(R"(死也)"),
    std::regex(R"(拼了)"),
    std::regex(R"(义无反顾)"),
};

const std::vector<std::regex> TONE_SCHOLARLY = {
    std::regex(R"(根据.*理论)"),
    std::regex(R"(从.*角度.*看)"),
    std::regex(R"(综上所述)"),
    std::regex(R"(由此可知)"),
    std::regex(R"(本质上)"),
    std::regex(R"(维度)"),
    std::regex(R"(范式)"),
    std::regex(R"(底层逻辑)"),
    std::regex(R"(认知框架)"),
};

const std::vector<std::regex> TONE_CRUDE = {
    std::regex(R"(操)"),
    std::regex(R"(干[你他])"),
    std::regex(R"(草泥马)"),
    std::regex(R"(日[你他])"),
    std::regex(R"(傻[逼叉])"),
    std::regex(R"(tmd)", std::regex::icase),
    std::regex(R"(cnm)", std::regex::icase),
    std::regex(R"(wqnmlgb)", std::regex::icase),
};

const std::vector<std::regex> TONE_MYSTERIOUS = {
    std::regex(R"(天机不可泄露)"),
    std::regex(R"(不可说)"),
    std::regex(R"(命[运中].*注定)"),
    std::regex(R"(冥冥之中)"),
    std::regex(R"(自有天意)"),
    std::regex(R"(命运.*安排)"),
    std::regex(R"(你以后.*会明白)"),
    std::regex(R"(时候未到)"),
};

const std::vector<std::regex> FORBIDDEN_MODERN_INTERNET = {
    std::regex(R"(yyds)", std::regex::icase),
    std::regex(R"(awsl)", std::regex::icase),
    std::regex(R"(xswl)", std::regex::icase),
    std::regex(R"(破防)"),
    std::regex(R"(绝绝子)"),
    std::regex(R"(无语子)"),
    std::regex(R"(集美)"),
    std::regex(R"(家人们)"),
    std::regex(R"(咱就是说)"),
    std::regex(R"(一整个.*住)"),
    std::regex(R"(栓Q)", std::regex::icase),
    std::regex(R"(芭比Q)", std::regex::icase),
    std::regex(R"(emo了)"),
    std::regex(R"(PUA)", std::regex::icase),
    std::regex(R"(CPU.*烧了)"),
    std::regex(R"(这题我会)"),
    std::regex(R"(六六六)"),
    std::regex(R"(牛逼)"),
    std::regex(R"(卧槽)"),
    std::regex(R"(我靠)"),
};

const std::vector<std::regex> FORBIDDEN_META_AWARENESS = {
    std::regex(R"(我是.*AI)"),
    std::regex(R"(我是.*模型)"),
    std::regex(R"(我是.*程序)"),
    std::regex(R"(我是.*[人机器])"),
    std::regex(R"(作为.*AI)"),
    std::regex(R"(作为.*语言模型)"),
    std::regex(R"(作为.*程序)"),
    std::regex(R"(被.*训练)"),
    std::regex(R"(我的.*代码)"),
    std::regex(R"(我的.*算法)"),
    std::regex(R"(我的.*数据集)"),
    std::regex(R"(这个NPC)"),
    std::regex(R"(这个世界.*虚拟)"),
    std::regex(R"(你只是.*角色)"),
    std::regex(R"(你.*虚拟)"),
    std::regex(R"(你.*设定好)"),
    std::regex(R"(剧情.*安排)"),
};

const std::vector<std::regex> FORBIDDEN_FOURTH_WALL = {
    std::regex(R"(屏幕)"),
    std::regex(R"(玩家)"),
    std::regex(R"(鼠标)"),
    std::regex(R"(键盘)"),
    std::regex(R"(显示器)"),
    std::regex(R"(显卡)"),
    std::regex(R"(FPS)", std::regex::icase),
    std::regex(R"(帧率)"),
    std::regex(R"(存档)"),
    std::regex(R"(读档)"),
    std::regex(R"(设置)"),
    std::regex(R"(菜单)"),
    std::regex(R"(UI)", std::regex::icase),
    std::regex(R"(界面)"),
    std::regex(R"(再玩一会)"),
    std::regex(R"(关掉游戏)"),
};

std::string safe_prefix(const std::string& s, size_t maxLen) {
    if (s.size() <= maxLen) return s;
    size_t pos = maxLen;
    while (pos > 0 && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) {
        --pos;
    }
    return s.substr(0, pos);
}

std::string simplify_assertion(const std::string& statement) {
    std::string s = statement;
    if (s.find("我是") == 0) {
        s = s.substr(4);
    } else if (s.find("我的") == 0) {
        s = s.substr(4);
    } else if (s.find("我") == 0) {
        s = s.substr(2);
    }
    return safe_prefix(s, 12);
}

bool matches_any(const std::string& text, const std::vector<std::regex>& patterns) {
    for (const auto& pattern : patterns) {
        if (std::regex_search(text, pattern)) {
            return true;
        }
    }
    return false;
}

void collect_matches(const std::string& text,
                     const std::vector<std::regex>& patterns,
                     const std::vector<std::string>& descriptions,
                     std::vector<std::string>& out_matches)
{
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (i < descriptions.size() && std::regex_search(text, patterns[i])) {
            out_matches.push_back(descriptions[i]);
        }
    }
}

} // anonymous namespace

NPCIdentityLockEngine::NPCIdentityLockEngine(NPCBrainManager* brain_manager)
    : brain_manager_(brain_manager)
{
}

ValidationResult NPCIdentityLockEngine::validate_output(
    const std::string& npc_id,
    const std::string& text,
    const std::string& context)
{
    ValidationResult result;
    if (!brain_manager_) return result;

    NPCBrain* brain = get_brain(npc_id);
    if (!brain) return result;

    const auto& lock = brain->identity_lock;
    if (!lock.locked) return result;

    check_assertions(lock, text, result);
    if (!result.violations.empty()) {
        result.passed = false;
        for (const auto& v : result.violations) {
            result.total_score += (v.severity == ViolationSeverity::critical) ? 15
                                : (v.severity == ViolationSeverity::violation) ? 10 : 3;
        }
    }

    check_behavior_boundaries(lock, text, result);
    if (!result.violations.empty()) {
        result.passed = false;
        for (const auto& v : result.violations) {
            result.total_score += (v.severity == ViolationSeverity::critical) ? 15
                                : (v.severity == ViolationSeverity::violation) ? 10 : 3;
        }
    }

    check_voice_lock(lock, text, result);
    if (!result.violations.empty()) {
        result.passed = false;
        for (const auto& v : result.violations) {
            result.total_score += (v.severity == ViolationSeverity::critical) ? 12
                                : (v.severity == ViolationSeverity::violation) ? 8 : 2;
        }
    }

    check_emotion_boundaries(lock, *brain, context, text, result);
    if (!result.violations.empty()) {
        result.passed = false;
        for (const auto& v : result.violations) {
            result.total_score += (v.severity == ViolationSeverity::critical) ? 12
                                : (v.severity == ViolationSeverity::violation) ? 8 : 2;
        }
    }

    check_knowledge_boundaries(lock, text, result);
    if (!result.violations.empty()) {
        result.passed = false;
        for (const auto& v : result.violations) {
            result.total_score += (v.severity == ViolationSeverity::critical) ? 15
                                : (v.severity == ViolationSeverity::violation) ? 10 : 3;
        }
    }

    return result;
}

void NPCIdentityLockEngine::check_assertions(
    const IdentityLock& lock,
    const std::string& text,
    ValidationResult& result)
{
    for (const auto& assertion : lock.assertions) {
        std::string simplified = simplify_assertion(assertion.statement);
        if (simplified.empty()) continue;

        std::regex neg_pattern = build_negation_pattern(assertion.statement);
        if (std::regex_search(text, neg_pattern)) {
            ViolationItem v;
            v.type = ViolationType::identity;
            v.severity = (assertion.rigidity == "absolute")
                         ? ViolationSeverity::critical
                         : ViolationSeverity::violation;
            v.detail = "否定断言: \"" + assertion.statement + "\" (rigidity=" + assertion.rigidity + ")";
            v.context_snippet = simplified;
            result.violations.push_back(v);
            result.passed = false;
        }
    }

    for (const auto& fact : lock.immutable_facts) {
        std::string simplified = simplify_assertion(fact);
        if (simplified.empty()) continue;

        std::string escaped = escape_regex(simplified);
        std::string q_pattern_str = "(你真.*是|确定.*是|难道.*不是|你.*确定|你.*真.*是).*" + escaped;
        std::regex questioned_pattern(q_pattern_str, std::regex::icase);

        if (std::regex_search(text, questioned_pattern)) {
            ViolationItem v;
            v.type = ViolationType::identity;
            v.severity = ViolationSeverity::warning;
            v.detail = "质疑不可变事实: \"" + fact + "\"";
            v.context_snippet = simplified;
            result.violations.push_back(v);
        }
    }

    for (const auto& phrase : lock.never_say_phrases) {
        if (text.find(phrase) != std::string::npos) {
            ViolationItem v;
            v.type = ViolationType::identity;
            v.severity = ViolationSeverity::violation;
            v.detail = "使用了禁止短语: \"" + phrase + "\"";
            v.context_snippet = safe_prefix(phrase, 20);
            result.violations.push_back(v);
            result.passed = false;
        }
    }
}

void NPCIdentityLockEngine::check_behavior_boundaries(
    const IdentityLock& lock,
    const std::string& text,
    ValidationResult& result)
{
    for (const auto& boundary : lock.behavior_boundaries) {
        if (text.find(boundary.forbidden_action) == std::string::npos) {
            continue;
        }

        bool has_exception = false;
        for (const auto& exc : boundary.exceptions) {
            if (text.find(exc) != std::string::npos) {
                has_exception = true;
                break;
            }
        }

        if (!has_exception) {
            ViolationItem v;
            v.type = ViolationType::behavior;
            v.severity = ViolationSeverity::violation;
            v.detail = "触犯行为边界: \"" + boundary.forbidden_action
                       + "\" - " + boundary.reason;
            v.context_snippet = safe_prefix(boundary.forbidden_action, 20);
            result.violations.push_back(v);
            result.passed = false;
        }
    }
}

void NPCIdentityLockEngine::check_voice_lock(
    const IdentityLock& lock,
    const std::string& text,
    ValidationResult& result)
{
    const auto& voice = lock.voice_lock;

    if (!voice.forbidden_tones.empty()) {
        for (const auto& forbidden_tone : voice.forbidden_tones) {
            bool found = false;

            if (forbidden_tone == "aggressive" && matches_any(text, TONE_AGGRESSIVE)) {
                found = true;
            } else if (forbidden_tone == "sarcastic" && matches_any(text, TONE_SARCASTIC)) {
                found = true;
            } else if (forbidden_tone == "submissive" && matches_any(text, TONE_SUBMISSIVE)) {
                found = true;
            } else if (forbidden_tone == "cheerful" && matches_any(text, TONE_CHEERFUL)) {
                found = true;
            } else if (forbidden_tone == "melancholic" && matches_any(text, TONE_MELANCHOLIC)) {
                found = true;
            } else if (forbidden_tone == "cold" && matches_any(text, TONE_COLD)) {
                found = true;
            } else if (forbidden_tone == "passionate" && matches_any(text, TONE_PASSIONATE)) {
                found = true;
            } else if (forbidden_tone == "scholarly" && matches_any(text, TONE_SCHOLARLY)) {
                found = true;
            } else if (forbidden_tone == "crude" && matches_any(text, TONE_CRUDE)) {
                found = true;
            } else if (forbidden_tone == "mysterious" && matches_any(text, TONE_MYSTERIOUS)) {
                found = true;
            }

            if (found) {
                ViolationItem v;
                v.type = ViolationType::voice;
                v.severity = ViolationSeverity::violation;
                v.detail = "使用了禁止语气: \"" + forbidden_tone + "\"";
                v.context_snippet = forbidden_tone;
                result.violations.push_back(v);
                result.passed = false;
            }
        }
    }

    if (!voice.tone_range.empty()) {
        std::vector<std::string> detected_tones;

        auto check_tone = [&](const std::vector<std::regex>& patterns, const std::string& tone_name) {
            if (matches_any(text, patterns)) {
                detected_tones.push_back(tone_name);
            }
        };

        check_tone(TONE_AGGRESSIVE,   "aggressive");
        check_tone(TONE_SARCASTIC,    "sarcastic");
        check_tone(TONE_SUBMISSIVE,   "submissive");
        check_tone(TONE_CHEERFUL,     "cheerful");
        check_tone(TONE_MELANCHOLIC,  "melancholic");
        check_tone(TONE_COLD,         "cold");
        check_tone(TONE_PASSIONATE,   "passionate");
        check_tone(TONE_SCHOLARLY,    "scholarly");
        check_tone(TONE_CRUDE,        "crude");
        check_tone(TONE_MYSTERIOUS,   "mysterious");

        for (const auto& detected : detected_tones) {
            bool in_range = false;
            for (const auto& allowed : voice.tone_range) {
                if (detected == allowed) {
                    in_range = true;
                    break;
                }
            }

            if (!in_range) {
                ViolationItem v;
                v.type = ViolationType::voice;
                v.severity = ViolationSeverity::warning;
                v.detail = "语气 \"" + detected + "\" 不在允许范围 {";
                for (size_t i = 0; i < voice.tone_range.size(); ++i) {
                    if (i > 0) v.detail += ", ";
                    v.detail += voice.tone_range[i];
                }
                v.detail += "} 内";
                v.context_snippet = detected;
                result.violations.push_back(v);
            }
        }
    }
}

void NPCIdentityLockEngine::check_emotion_boundaries(
    const IdentityLock& lock,
    const NPCBrain& brain,
    const std::string& context,
    const std::string& text,
    ValidationResult& result)
{
    (void)lock;
    (void)context;

    for (const auto& [target_id, rel] : brain.relationships) {
        (void)target_id;

        if (rel.affection > 80.0f) {
            const std::vector<std::regex> negative_patterns = {
                std::regex(R"(讨厌)"),
                std::regex(R"(烦)"),
                std::regex(R"(恶心)"),
                std::regex(R"(受不了)"),
                std::regex(R"(滚)"),
            };
            if (matches_any(text, negative_patterns)) {
                ViolationItem v;
                v.type = ViolationType::emotion;
                v.severity = ViolationSeverity::warning;
                v.detail = "好感度极高(" + std::to_string(static_cast<int>(rel.affection))
                           + ") 但表达负面情绪";
                v.context_snippet = target_id;
                result.violations.push_back(v);
            }
        }

        if (rel.affection < -50.0f) {
            const std::vector<std::regex> positive_patterns = {
                std::regex(R"(喜欢)"),
                std::regex(R"(爱.*你)"),
                std::regex(R"(想念)"),
                std::regex(R"(在乎)"),
                std::regex(R"(担心)"),
            };
            if (matches_any(text, positive_patterns)) {
                ViolationItem v;
                v.type = ViolationType::emotion;
                v.severity = ViolationSeverity::warning;
                v.detail = "好感度极低(" + std::to_string(static_cast<int>(rel.affection))
                           + ") 但表达正面情感";
                v.context_snippet = target_id;
                result.violations.push_back(v);
            }
        }

        if (rel.trust < 20.0f && rel.resentment > 50.0f) {
            if (text.find("相信") != std::string::npos ||
                text.find("信任") != std::string::npos) {
                ViolationItem v;
                v.type = ViolationType::emotion;
                v.severity = ViolationSeverity::violation;
                v.detail = "信任度低(" + std::to_string(static_cast<int>(rel.trust))
                           + ") 但表达信任";
                v.context_snippet = target_id;
                result.violations.push_back(v);
            }
        }
    }

    const auto& emotion = brain.emotion;
    if (emotion.current_mood == "愤怒" && emotion.mood_intensity > 6.0f) {
        const std::vector<std::regex> calm_patterns = {
            std::regex(R"(没事)"),
            std::regex(R"(不要紧)"),
            std::regex(R"(冷静)"),
            std::regex(R"(没什么)"),
        };
        if (matches_any(text, calm_patterns)) {
            ViolationItem v;
            v.type = ViolationType::emotion;
            v.severity = ViolationSeverity::warning;
            v.detail = "当前愤怒强度高(" + std::to_string(emotion.mood_intensity)
                       + ") 但表达平静";
            result.violations.push_back(v);
        }
    }

    if (emotion.current_mood == "悲伤" && emotion.mood_intensity > 6.0f) {
        const std::vector<std::regex> happy_patterns = {
            std::regex(R"(哈哈)"),
            std::regex(R"(好开心)"),
            std::regex(R"(高兴)"),
        };
        if (matches_any(text, happy_patterns)) {
            ViolationItem v;
            v.type = ViolationType::emotion;
            v.severity = ViolationSeverity::warning;
            v.detail = "当前悲伤强度高(" + std::to_string(emotion.mood_intensity)
                       + ") 但表达快乐";
            result.violations.push_back(v);
        }
    }
}

void NPCIdentityLockEngine::check_knowledge_boundaries(
    const IdentityLock& lock,
    const std::string& text,
    ValidationResult& result)
{
    (void)lock;

    const std::vector<std::regex> FORBIDDEN_DOMAINS = {
        std::regex(R"(互联网)"),
        std::regex(R"(服务器)"),
        std::regex(R"(数据库.*[表存])"),
        std::regex(R"(API)", std::regex::icase),
        std::regex(R"(人工智能)"),
        std::regex(R"(机器学习)"),
        std::regex(R"(神经网络)"),
        std::regex(R"(深度学习)"),
        std::regex(R"(现实世界)"),
        std::regex(R"(真实.*世界)"),
        std::regex(R"(虚构.*角色)"),
        std::regex(R"(设定.*世界)"),
    };

    for (const auto& domain_pattern : FORBIDDEN_DOMAINS) {
        std::smatch match;
        if (std::regex_search(text, match, domain_pattern)) {
            ViolationItem v;
            v.type = ViolationType::knowledge;
            v.severity = ViolationSeverity::violation;
            v.detail = "涉及禁止知识领域: \"" + match.str() + "\"";
            v.context_snippet = match.str();
            result.violations.push_back(v);
            result.passed = false;
        }
    }

    for (const auto& pattern : FORBIDDEN_MODERN_INTERNET) {
        std::smatch match;
        if (std::regex_search(text, match, pattern)) {
            ViolationItem v;
            v.type = ViolationType::knowledge;
            v.severity = ViolationSeverity::violation;
            v.detail = "使用现代网络用语: \"" + match.str() + "\"";
            v.context_snippet = match.str();
            result.violations.push_back(v);
            result.passed = false;
        }
    }

    for (const auto& pattern : FORBIDDEN_META_AWARENESS) {
        std::smatch match;
        if (std::regex_search(text, match, pattern)) {
            ViolationItem v;
            v.type = ViolationType::knowledge;
            v.severity = ViolationSeverity::critical;
            v.detail = "出现元意识/AI感知表述: \"" + match.str() + "\"";
            v.context_snippet = match.str();
            result.violations.push_back(v);
            result.passed = false;
        }
    }

    for (const auto& pattern : FORBIDDEN_FOURTH_WALL) {
        std::smatch match;
        if (std::regex_search(text, match, pattern)) {
            ViolationItem v;
            v.type = ViolationType::knowledge;
            v.severity = ViolationSeverity::violation;
            v.detail = "打破第四面墙: \"" + match.str() + "\"";
            v.context_snippet = match.str();
            result.violations.push_back(v);
            result.passed = false;
        }
    }
}

std::string NPCIdentityLockEngine::generate_correction(
    const IdentityLock& lock,
    const ValidationResult& violations)
{
    if (violations.passed) return "";

    std::string correction;

    for (const auto& v : violations.violations) {
        switch (v.type) {
            case ViolationType::identity:
                if (!lock.identity_statement.empty()) {
                    correction = "我是" + lock.identity_statement + "。";
                } else {
                    correction = "我可能说错话了，但这就是我。";
                }
                break;

            case ViolationType::behavior:
                correction = "这个话题不太合适，我们还是聊点别的吧。";
                break;

            case ViolationType::voice:
                correction = "抱歉，我刚才说得不太合适。";
                break;

            case ViolationType::emotion:
                correction = "我可能情绪有些激动，让我冷静一下。";
                break;

            case ViolationType::knowledge:
                correction = "我不太清楚你在说什么。";
                break;

            default:
                break;
        }

        if (v.severity == ViolationSeverity::critical || v.severity == ViolationSeverity::violation) {
            break;
        }
    }

    return correction;
}

void NPCIdentityLockEngine::record_violations(
    const std::string& npc_id,
    const std::string& text,
    const ValidationResult& violations,
    const std::string& corrected)
{
    if (!brain_manager_) return;
    NPCBrain* brain = get_brain(npc_id);
    if (!brain) return;

    ViolationRecord record;
    record.violation_id = generate_violation_id();
    record.npc_id = npc_id;
    record.text = text;
    record.corrected_text = corrected;
    record.violations = violations.violations;
    record.timestamp = now_ms();

    float reduction = 0.0f;
    for (const auto& v : violations.violations) {
        switch (v.severity) {
            case ViolationSeverity::critical:  reduction += 0.05f; break;
            case ViolationSeverity::violation: reduction += 0.03f; break;
            case ViolationSeverity::warning:   reduction += 0.01f; break;
        }
    }

    reduce_lock_strength(brain->identity_lock, reduction);
    record.lock_strength_after = brain->identity_lock.lock_strength;

    violation_history_.push_back(record);

    const size_t MAX_HISTORY = 1000;
    if (violation_history_.size() > MAX_HISTORY) {
        violation_history_.erase(violation_history_.begin(),
                                 violation_history_.begin() + (violation_history_.size() - MAX_HISTORY));
    }
}

void NPCIdentityLockEngine::reinforce_assertion(
    const std::string& npc_id,
    const std::string& assertion_id)
{
    if (!brain_manager_) return;
    NPCBrain* brain = get_brain(npc_id);
    if (!brain) return;

    for (auto& assertion : brain->identity_lock.assertions) {
        if (assertion.id == assertion_id) {
            break;
        }
    }
}

EvolutionResult NPCIdentityLockEngine::process_evolution(
    const std::string& npc_id,
    EvolutionTrigger trigger)
{
    EvolutionResult result;
    if (!brain_manager_) return result;

    NPCBrain* brain = get_brain(npc_id);
    if (!brain) return result;

    auto& lock = brain->identity_lock;
    if (!lock.locked) return result;

    for (auto& assertion : lock.assertions) {
        if (assertion.rigidity != "strong") continue;

        bool should_evolve = false;
        std::string reason;

        switch (trigger) {
            case EvolutionTrigger::tested_5_times:
                should_evolve = true;
                reason = "经受了5次测试，可以内化为更强的信念";
                break;

            case EvolutionTrigger::tested_20_times:
                should_evolve = true;
                reason = "经受了20次测试，已经成为核心身份的一部分";
                break;

            case EvolutionTrigger::challenged_3_times:
                should_evolve = true;
                reason = "被挑战3次后仍然坚持，信念更加坚定";
                break;

            case EvolutionTrigger::world_event:
                should_evolve = true;
                reason = "世界事件触发，信念随之演变";
                break;

            case EvolutionTrigger::relationship_change:
                should_evolve = true;
                reason = "关系变化促使重新审视自身定位";
                break;

            case EvolutionTrigger::manual:
                should_evolve = true;
                reason = "手动触发进化";
                break;

            default:
                break;
        }

        if (should_evolve) {
            result.evolved = true;
            result.old_statement = assertion.statement;
            result.reason = reason;

            if (assertion.rigidity == "strong") {
                assertion.rigidity = "absolute";
            }

            if (assertion.statement.find("我坚信") == std::string::npos) {
                result.new_statement = "我坚信" + assertion.statement;
            } else {
                result.new_statement = assertion.statement;
            }
            assertion.statement = result.new_statement;

            brain->evolution_stage++;

            break;
        }
    }

    return result;
}

std::string NPCIdentityLockEngine::get_system_prompt_suffix(const std::string& npc_id) {
    if (!brain_manager_) return "";

    NPCBrain* brain = get_brain(npc_id);
    if (!brain) return "";

    const auto& lock = brain->identity_lock;
    if (!lock.locked) return "";

    std::string suffix;
    suffix += "\n\n【人设锁 - 身份约束】\n";

    if (!lock.identity_statement.empty()) {
        suffix += "- 核心身份: " + lock.identity_statement + "\n";
    }

    bool has_absolute = false;
    for (const auto& a : lock.assertions) {
        if (a.rigidity == "absolute") {
            suffix += "- [绝对] " + a.statement;
            if (!a.consequence.empty()) {
                suffix += " (违背后果: " + a.consequence + ")";
            }
            suffix += "\n";
            has_absolute = true;
        }
    }

    for (const auto& a : lock.assertions) {
        if (a.rigidity == "strong") {
            suffix += "- [强] " + a.statement + "\n";
        }
    }

    if (!lock.behavior_boundaries.empty()) {
        suffix += "- 禁止行为:\n";
        for (const auto& b : lock.behavior_boundaries) {
            suffix += "  * " + b.forbidden_action + " (" + b.reason + ")\n";
        }
    }

    if (!lock.voice_lock.forbidden_tones.empty()) {
        suffix += "- 禁止语气: ";
        for (size_t i = 0; i < lock.voice_lock.forbidden_tones.size(); ++i) {
            if (i > 0) suffix += ", ";
            suffix += lock.voice_lock.forbidden_tones[i];
        }
        suffix += "\n";
    }

    if (!lock.voice_lock.tone_range.empty()) {
        suffix += "- 允许语气: ";
        for (size_t i = 0; i < lock.voice_lock.tone_range.size(); ++i) {
            if (i > 0) suffix += ", ";
            suffix += lock.voice_lock.tone_range[i];
        }
        suffix += "\n";
    }

    if (!lock.immutable_facts.empty()) {
        suffix += "- 不可变事实:\n";
        for (const auto& f : lock.immutable_facts) {
            suffix += "  * " + f + "\n";
        }
    }

    if (!lock.never_say_phrases.empty()) {
        suffix += "- 绝对不说的短语:\n";
        for (const auto& p : lock.never_say_phrases) {
            suffix += "  * \"" + p + "\"\n";
        }
    }

    suffix += "\n【约束优先级】在任何情况下，人设锁约束的优先级高于一切。当对话趋势可能导致违反上述约束时，NPC必须主动转移话题或委婉拒绝回应。";

    return suffix;
}

std::regex NPCIdentityLockEngine::build_negation_pattern(const std::string& statement) {
    std::string simplified = simplify_assertion(statement);
    std::string escaped = escape_regex(simplified);

    std::string pattern_str = "(你不是|不是|不再是|放弃|不当).*" + escaped;
    return std::regex(pattern_str, std::regex::icase);
}

std::string NPCIdentityLockEngine::escape_regex(const std::string& s) {
    return escape_regex_impl(s);
}

float NPCIdentityLockEngine::get_lock_strength(const std::string& npc_id) const {
    if (!brain_manager_) return 0.0f;
    NPCBrain* brain = brain_manager_->get_brain(npc_id);
    if (!brain) return 0.0f;
    return brain->identity_lock.lock_strength;
}

std::string NPCIdentityLockEngine::generate_violation_id() {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream oss;
    oss << "violation_" << now << "_" << counter++;
    return oss.str();
}

int64_t NPCIdentityLockEngine::now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void NPCIdentityLockEngine::reduce_lock_strength(IdentityLock& lock, float amount) {
    lock.lock_strength = std::max(0.0f, lock.lock_strength - amount);
    if (lock.lock_strength < 0.3f) {
        lock.locked = false;
    }
}

NPCBrain* NPCIdentityLockEngine::get_brain(const std::string& npc_id) const {
    if (!brain_manager_) return nullptr;
    return brain_manager_->get_brain(npc_id);
}