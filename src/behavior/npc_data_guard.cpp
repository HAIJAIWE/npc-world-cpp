#include "npc_data_guard.h"
#include "npc_brain.h"
#include <algorithm>
#include <cstdio>
#include <ctime>

NPCDataGuard::NPCDataGuard(NPCBrainManager* brain_manager)
    : brain_mgr_(brain_manager) {}

std::string NPCDataGuard::get_npc_name(const std::string& npc_id) {
    if (!brain_mgr_) return npc_id;
    NPCBrain* brain = brain_mgr_->get_brain(npc_id);
    if (!brain) return npc_id;
    return brain->personality.name.empty() ? npc_id : brain->personality.name;
}

IntegrityReport NPCDataGuard::check_integrity(const std::string& npc_id) {
    IntegrityReport report;
    report.npc_id = npc_id;

    if (!brain_mgr_) {
        report.npc_name = "未知";
        report.healthy = false;
        report.issues.push_back("大脑管理器未初始化");
        report.fixes.push_back("请先初始化NPCBrainManager");
        return report;
    }

    NPCBrain* brain = brain_mgr_->get_brain(npc_id);
    if (!brain) {
        report.npc_name = "未知";
        report.healthy = false;
        report.issues.push_back("大脑数据不存在");
        report.fixes.push_back("需要重新初始化NPC");
        return report;
    }

    report.npc_name = get_npc_name(npc_id);

    if (brain->personality.name.empty()) {
        report.issues.push_back("身份名称缺失");
        report.fixes.push_back("重新初始化身份数据");
    }
    if (brain->personality.base_traits.empty()) {
        report.issues.push_back("人格特质缺失");
        report.fixes.push_back("重新初始化人格数据");
    }

    if (brain->emotion.immediate_emotion.type.empty()) {
        report.issues.push_back("瞬时情绪数据缺失");
        report.fixes.push_back("补充默认情绪数据");
    }
    if (brain->emotion.background_mood.label.empty()) {
        report.issues.push_back("心境数据缺失");
        report.fixes.push_back("补充默认心境数据");
    }

    if (brain->self_concept.self_image.empty()) {
        report.issues.push_back("自我形象数据缺失");
        report.fixes.push_back("补充默认自我数据");
    }
    if (brain->self_concept.goals.empty()) {
        report.issues.push_back("人生目标数据缺失");
        report.fixes.push_back("补充默认目标数据");
    }

    int total_mem = (int)brain->short_term_memory.size() +
                    (int)brain->long_term_memory.size() +
                    (int)brain->core_memory.size();
    if (total_mem > 500) {
        report.issues.push_back("记忆膨胀：" + std::to_string(total_mem) + "条");
        report.fixes.push_back("建议自动裁剪低频记忆");
    }

    if (brain->mental_energy <= 0.0f) {
        report.issues.push_back("精力数据异常（为0）");
        report.fixes.push_back("重置精力值为最大值");
    }
    if (brain->max_mental_energy <= 0.0f) {
        report.issues.push_back("最大精力值异常");
        report.fixes.push_back("重置最大精力值");
    }

    if (brain->worldview.description.empty()) {
        report.issues.push_back("世界观描述缺失");
        report.fixes.push_back("补充默认世界观");
    }

    report.stats.short_term = (int)brain->short_term_memory.size();
    report.stats.long_term = (int)brain->long_term_memory.size();
    report.stats.core = (int)brain->core_memory.size();
    report.stats.knowledge = (int)brain->knowledge.size();
    report.stats.reflections = (int)brain->reflections.size();
    report.stats.relationships = (int)brain->relationships.size();
    report.stats.ruminations = (int)brain->conflicts.core_conflict_pairs.size();
    report.stats.total_memories = total_mem;

    report.healthy = report.issues.empty();

    return report;
}

std::vector<std::string> NPCDataGuard::repair_data(const IntegrityReport& report) {
    std::vector<std::string> applied_fixes;

    if (!brain_mgr_) return applied_fixes;

    NPCBrain* brain = brain_mgr_->get_brain(report.npc_id);
    if (!brain) return applied_fixes;

    if (brain->personality.name.empty()) {
        brain->personality.name = report.npc_id;
        applied_fixes.push_back("补充NPC名称为ID: " + report.npc_id);
    }

    if (brain->personality.base_traits.empty()) {
        brain->personality.base_traits = {"普通", "平和"};
        applied_fixes.push_back("补充默认人格特质");
    }

    if (brain->emotion.immediate_emotion.type.empty()) {
        brain->emotion.immediate_emotion.type = "平静";
        brain->emotion.immediate_emotion.intensity = 3.0f;
        brain->emotion.immediate_emotion.decay_rate = 0.15f;
        brain->emotion.immediate_emotion.started_at = std::time(nullptr);
        applied_fixes.push_back("修复情绪状态数据");
    }

    if (brain->emotion.background_mood.label.empty()) {
        brain->emotion.background_mood.label = "平常";
        brain->emotion.background_mood.valence = 0.0f;
        brain->emotion.background_mood.arousal = 3.0f;
        applied_fixes.push_back("修复背景心境数据");
    }

    if (brain->self_concept.self_image.empty()) {
        brain->self_concept.self_image = "一个普通人";
        brain->self_concept.identity = brain->personality.role.empty() ? "路人" : brain->personality.role;
        applied_fixes.push_back("修复自我数据");
    }

    if (brain->worldview.description.empty()) {
        brain->worldview.world_nature = "mixed";
        brain->worldview.human_nature = "mixed";
        brain->worldview.justice = "exists";
        brain->worldview.description = "一个普通的世界，有好有坏。";
        applied_fixes.push_back("修复世界观数据");
    }

    if (brain->mental_energy <= 0.0f) {
        brain->mental_energy = brain->max_mental_energy > 0.0f ? brain->max_mental_energy : 100.0f;
        applied_fixes.push_back("重置精力值");
    }
    if (brain->max_mental_energy <= 0.0f) {
        brain->max_mental_energy = 100.0f;
        brain->mental_energy = 100.0f;
        applied_fixes.push_back("重置最大精力值为100");
    }

    if (brain->short_term_memory.size() > 100) {
        std::sort(brain->short_term_memory.begin(), brain->short_term_memory.end(),
                  [](const BrainMemory& a, const BrainMemory& b) { return a.importance > b.importance; });
        brain->short_term_memory.resize(80);
        applied_fixes.push_back("裁剪短期记忆到80条");
    }
    if (brain->long_term_memory.size() > 200) {
        std::sort(brain->long_term_memory.begin(), brain->long_term_memory.end(),
                  [](const BrainMemory& a, const BrainMemory& b) { return a.importance > b.importance; });
        brain->long_term_memory.resize(180);
        applied_fixes.push_back("裁剪长期记忆到180条");
    }

    return applied_fixes;
}

std::vector<IntegrityReport> NPCDataGuard::scan_all_npcs() {
    std::vector<IntegrityReport> reports;
    if (!brain_mgr_) return reports;
    return reports;
}