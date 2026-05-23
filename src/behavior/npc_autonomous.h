#pragma once
#include "npc_brain.h"
#include "npc_brain_types.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>

enum class AutonomousActionType : uint8_t {
    Eat, Drink, Rest, Sleep, Wake, Socialize, Wander,
    Craft, Read, Exercise, Meditate, Gaze, Complain,
    Reminisce, Plan, Idle
};

struct AutonomousAction {
    AutonomousActionType type = AutonomousActionType::Idle;
    std::string description;
    std::string thought;
    std::string target_npc_id;
    float duration = 30.0f;
};

struct ActiveAction {
    AutonomousAction action;
    int64_t started_at = 0;
};

inline const std::unordered_map<AutonomousActionType, std::vector<std::pair<AutonomousActionType, float>>> NEED_ACTIONS{
    {AutonomousActionType::Eat,        {{AutonomousActionType::Eat, 1.0f}}},
    {AutonomousActionType::Sleep,      {{AutonomousActionType::Sleep, 0.7f}, {AutonomousActionType::Rest, 0.3f}}},
    {AutonomousActionType::Rest,       {{AutonomousActionType::Rest, 0.7f}, {AutonomousActionType::Drink, 0.3f}}},
    {AutonomousActionType::Socialize,  {{AutonomousActionType::Socialize, 0.7f}, {AutonomousActionType::Wander, 0.3f}}},
    {AutonomousActionType::Wander,     {{AutonomousActionType::Wander, 0.3f}, {AutonomousActionType::Craft, 0.3f}, {AutonomousActionType::Read, 0.2f}, {AutonomousActionType::Exercise, 0.2f}}},
    {AutonomousActionType::Meditate,   {{AutonomousActionType::Meditate, 0.3f}, {AutonomousActionType::Rest, 0.3f}, {AutonomousActionType::Wander, 0.2f}, {AutonomousActionType::Complain, 0.2f}}},
    {AutonomousActionType::Gaze,       {{AutonomousActionType::Socialize, 0.5f}, {AutonomousActionType::Wander, 0.3f}, {AutonomousActionType::Idle, 0.2f}}},
};

struct IdleActionWeight {
    AutonomousActionType action;
    float weight;
};

inline const std::vector<IdleActionWeight> IDLE_ACTIONS{
    {AutonomousActionType::Idle, 0.25f},
    {AutonomousActionType::Gaze, 0.15f},
    {AutonomousActionType::Wander, 0.15f},
    {AutonomousActionType::Reminisce, 0.15f},
    {AutonomousActionType::Plan, 0.15f},
    {AutonomousActionType::Read, 0.08f},
    {AutonomousActionType::Craft, 0.07f},
};

inline const std::unordered_map<AutonomousActionType, std::vector<std::string>> ACTION_DESCRIPTIONS{
    {AutonomousActionType::Eat,        {"去找点吃的", "肚子饿了，去厨房看看", "该吃点东西了"}},
    {AutonomousActionType::Drink,      {"有点渴，去喝点水", "去找点喝的"}},
    {AutonomousActionType::Rest,       {"有点累了，坐下来歇会儿", "找个地方坐坐", "该休息一下了"}},
    {AutonomousActionType::Sleep,      {"困得不行了，去睡觉", "太累了，回房间躺下"}},
    {AutonomousActionType::Wake,       {"睡醒了，精神不错", "天亮了，该起来了"}},
    {AutonomousActionType::Socialize,  {"想找个人聊聊天", "去看看有没有人在", "去看看{target}在做什么"}},
    {AutonomousActionType::Wander,     {"到处走走看看", "去散散步", "四处溜达一下"}},
    {AutonomousActionType::Craft,      {"做点手工活", "打磨一下我的作品", "搞点创作"}},
    {AutonomousActionType::Read,       {"找本书看看", "坐下来读会儿书"}},
    {AutonomousActionType::Exercise,   {"活动活动筋骨", "做几个俯卧撑"}},
    {AutonomousActionType::Meditate,   {"静下心来想想", "发会儿呆"}},
    {AutonomousActionType::Gaze,       {"望着窗外发呆", "看着远处出神"}},
    {AutonomousActionType::Complain,   {"自言自语抱怨了几句", "嘟囔着发泄了一下"}},
    {AutonomousActionType::Reminisce,  {"想起了以前的事", "回忆涌上心头"}},
    {AutonomousActionType::Plan,       {"想着接下来要做什么", "盘算着之后的计划"}},
    {AutonomousActionType::Idle,       {"百无聊赖地站着", "不知道该干什么", "随手摆弄着东西"}},
};

inline const std::unordered_map<AutonomousActionType, float> ACTION_DURATIONS{
    {AutonomousActionType::Eat, 300.0f}, {AutonomousActionType::Drink, 60.0f},
    {AutonomousActionType::Rest, 180.0f}, {AutonomousActionType::Sleep, 28800.0f},
    {AutonomousActionType::Wake, 60.0f}, {AutonomousActionType::Socialize, 120.0f},
    {AutonomousActionType::Wander, 90.0f}, {AutonomousActionType::Craft, 240.0f},
    {AutonomousActionType::Read, 300.0f}, {AutonomousActionType::Exercise, 180.0f},
    {AutonomousActionType::Meditate, 120.0f}, {AutonomousActionType::Gaze, 60.0f},
    {AutonomousActionType::Complain, 30.0f}, {AutonomousActionType::Reminisce, 60.0f},
    {AutonomousActionType::Plan, 90.0f}, {AutonomousActionType::Idle, 45.0f},
};

class NPCAutonomousEngine {
public:
    explicit NPCAutonomousEngine(NPCBrainManager& brain_manager);

    void tick(NPCBrain& brain);

    AutonomousAction select_action(const NPCBrain& brain);

    void apply_action(NPCBrain& brain, const AutonomousAction& action);

    const ActiveAction* get_active_action(const std::string& npc_id) const;

    void perform_night_consolidation(NPCBrain& brain);

private:
    NPCBrainManager& brain_manager_;
    std::unordered_map<std::string, ActiveAction> active_actions_;
    std::unordered_map<std::string, AutonomousNeeds> last_needs_;

    void drift_needs(NPCBrain& brain, bool has_active_action);

    float apply_weight_modifiers(const NPCBrain& brain, AutonomousActionType action, float base_weight);

    std::string pick_target_for_socialize(const NPCBrain& brain);

    static std::mt19937& rng();

    static std::string random_description(AutonomousActionType type, const std::string& target_npc_id);

    static std::string action_thought(const std::string& description);
};
