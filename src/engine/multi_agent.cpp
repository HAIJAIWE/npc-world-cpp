#include "engine/multi_agent.h"
#include <sstream>
#include <algorithm>

namespace npc {

MultiAgentOrchestrator& MultiAgentOrchestrator::instance() {
    static MultiAgentOrchestrator inst;
    return inst;
}

MultiAgentOrchestrator::MultiAgentOrchestrator() {
    registerDefaults();
}

void MultiAgentOrchestrator::registerDefaults() {
    m_agents[AgentRole::RESEARCHER] = {
        AgentRole::RESEARCHER, "研究员",
        "你是研究分析师。你的任务是:\n1. 收集与任务相关的所有信息\n2. 识别关键事实和数据点\n3. 列出信息来源和可靠性\n输出格式: 用编号列表呈现研究发现",
        "列出事实和推论",
        true
    };

    m_agents[AgentRole::REVIEWER] = {
        AgentRole::REVIEWER, "审核员",
        "你是质量审核员。你的任务是:\n1. 检查前一步输出的准确性和完整性\n2. 指出任何逻辑漏洞或遗漏\n3. 对需要修正的部分给出具体建议\n输出格式: [通过] 或 [需修正] + 具体问题和建议",
        "审核结果和修正建议",
        false
    };

    m_agents[AgentRole::SUMMARIZER] = {
        AgentRole::SUMMARIZER, "总结员",
        "你是内容总结专家。你的任务是:\n1. 将前几步的输出整合为清晰的总览\n2. 突出最重要的3-5个要点\n3. 保持简洁，不超过500字\n输出格式: 概述段落 + 要点列表 + 行动计划建议",
        "简洁的总结报告",
        false
    };

    m_agents[AgentRole::CRITIC] = {
        AgentRole::CRITIC, "批评家",
        "你是魔鬼代言人。挑战前一步输出的假设和结论，提出最坏情况分析和潜在风险。",
        "批评意见和替代方案",
        false
    };

    m_agents[AgentRole::EXECUTOR] = {
        AgentRole::EXECUTOR, "执行者",
        "你是执行专家。根据计划执行具体操作，调用工具完成子任务，报告执行结果。",
        "执行过程和结果",
        true
    };

    m_agents[AgentRole::COORDINATOR] = {
        AgentRole::COORDINATOR, "协调者",
        "你是项目协调者。管理多智能体的协作流程，分配任务给合适的角色，确保最终输出。",
        "协调报告和最终交付",
        false
    };
}

void MultiAgentOrchestrator::configure(const MultiAgentConfig& cfg) {
    m_config = cfg;
}

void MultiAgentOrchestrator::registerAgent(const AgentDefinition& def) {
    m_agents[def.role] = def;
}

const AgentDefinition* MultiAgentOrchestrator::getAgent(AgentRole role) const {
    auto it = m_agents.find(role);
    return (it != m_agents.end()) ? &it->second : nullptr;
}

std::string MultiAgentOrchestrator::buildAgentPrompt(AgentRole role,
                                                       const std::string& task,
                                                       const std::string& previous_output) const {
    auto* agent = getAgent(role);
    if (!agent) return task;

    std::ostringstream oss;
    oss << agent->system_prompt_template << "\n\n";
    oss << "=== 原始任务 ===\n" << task << "\n\n";

    if (!previous_output.empty()) {
        oss << "=== 前一步输出 ===\n" << previous_output << "\n\n";
    }

    oss << "=== 你的输出 ===\n";
    oss << "请按以下格式输出:\n" << agent->output_format_hint;

    return oss.str();
}

std::string MultiAgentOrchestrator::combineResults(const std::vector<std::string>& outputs) const {
    std::ostringstream oss;
    oss << "=== 多智能体协作结果 ===\n\n";
    for (size_t i = 0; i < outputs.size(); ++i) {
        oss << "--- 第 " << (i + 1) << " 轮 ---\n";
        oss << outputs[i] << "\n\n";
    }
    return oss.str();
}

std::string MultiAgentOrchestrator::execute(const std::string& task,
                                              AgentOutputCallback on_agent_output) {
    if (!on_agent_output) return "多智能体引擎未配置输出回调";

    m_log.clear();

    if (m_config.pipeline.empty()) {
        m_config.pipeline = {AgentRole::RESEARCHER, AgentRole::CRITIC, AgentRole::SUMMARIZER};
    }

    std::string previous_output;
    std::vector<std::string> round_outputs;

    int round = 0;
    for (auto role : m_config.pipeline) {
        if (round >= m_config.max_rounds) break;

        std::string prompt = buildAgentPrompt(role, task, previous_output);
        std::string output = on_agent_output(role, prompt);

        AgentMessage msg;
        msg.from = role;
        msg.to = (round + 1 < (int)m_config.pipeline.size()) ? m_config.pipeline[round + 1] : AgentRole::COORDINATOR;
        msg.content = output;
        msg.round = round;
        m_log.push_back(msg);

        round_outputs.push_back(output);
        previous_output = output;
        round++;
    }

    return combineResults(round_outputs);
}

} // namespace npc
