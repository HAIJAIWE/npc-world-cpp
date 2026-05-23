#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <deque>

namespace npc {

enum class AgentRole : uint8_t {
    RESEARCHER,
    REVIEWER,
    SUMMARIZER,
    CRITIC,
    EXECUTOR,
    COORDINATOR
};

struct AgentDefinition {
    AgentRole role;
    std::string name;
    std::string system_prompt_template;
    std::string output_format_hint;
    bool requires_tools = false;
};

struct AgentMessage {
    AgentRole from;
    AgentRole to;
    std::string content;
    int round = 0;
};

struct MultiAgentConfig {
    int max_rounds = 10;
    bool verbose = false;
    std::vector<AgentRole> pipeline;
};

using AgentOutputCallback = std::function<std::string(AgentRole role, const std::string& prompt)>;

class MultiAgentOrchestrator {
public:
    static MultiAgentOrchestrator& instance();

    void configure(const MultiAgentConfig& cfg);

    std::string execute(const std::string& task,
                        AgentOutputCallback on_agent_output);

    void registerAgent(const AgentDefinition& def);
    const AgentDefinition* getAgent(AgentRole role) const;

    std::deque<AgentMessage> getConversationLog() const { return m_log; }
    void clearLog() { m_log.clear(); }

private:
    MultiAgentOrchestrator();

    std::string buildAgentPrompt(AgentRole role, const std::string& task,
                                  const std::string& previous_output) const;
    std::string combineResults(const std::vector<std::string>& outputs) const;

    MultiAgentConfig m_config;
    std::unordered_map<AgentRole, AgentDefinition> m_agents;
    std::deque<AgentMessage> m_log;

    void registerDefaults();
};

inline const char* agent_role_name(AgentRole r) {
    switch (r) {
        case AgentRole::RESEARCHER:  return "研究员";
        case AgentRole::REVIEWER:    return "审核员";
        case AgentRole::SUMMARIZER:  return "总结员";
        case AgentRole::CRITIC:      return "批评家";
        case AgentRole::EXECUTOR:    return "执行者";
        case AgentRole::COORDINATOR: return "协调者";
        default: return "未知";
    }
}

} // namespace npc
