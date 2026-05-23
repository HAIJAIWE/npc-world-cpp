#include "engine/react_loop.h"
#include "engine/api_client.h"
#include <sstream>
#include <regex>
#include <algorithm>

namespace npc {

std::string ReActResult::format_trace() const {
    std::ostringstream oss;
    oss << "ReAct Trace (" << total_attempts << " attempts):\n";
    for (size_t i = 0; i < trace.size(); ++i) {
        const auto& step = trace[i];
        oss << "  Step " << (i + 1) << ":\n";
        oss << "    Thought: " << step.thought << "\n";
        oss << "    Action: " << step.action;
        if (!step.action_input.empty()) oss << "(" << step.action_input << ")";
        oss << "\n";
        oss << "    Observation: " << (step.is_error ? "[ERROR] " : "") << step.observation << "\n";
    }
    return oss.str();
}

ReActLoop& ReActLoop::instance() {
    static ReActLoop inst;
    return inst;
}

ReActLoop::ReActLoop() {
    m_config.max_iterations = 5;
    m_config.max_reflection_retries = 2;
}

void ReActLoop::configure(const ReActConfig& cfg) {
    m_config = cfg;
}

static std::string parseActionName(const std::string& thought) {
    std::regex action_re(R"(Action:\s*(\w+))", std::regex::icase);
    std::smatch match;
    if (std::regex_search(thought, match, action_re)) {
        return match[1].str();
    }

    std::regex fn_re(R"(调用\s*(\w+)|call\s+(\w+)|use\s+(\w+))", std::regex::icase);
    if (std::regex_search(thought, match, fn_re)) {
        for (size_t i = 1; i < match.size(); ++i) {
            if (match[i].matched) return match[i].str();
        }
    }
    return "";
}

static std::string parseActionInput(const std::string& thought) {
    std::regex input_re(R"(Action Input:\s*(.+?)(?:\n|$))", std::regex::icase);
    std::smatch match;
    if (std::regex_search(thought, match, input_re)) {
        return match[1].str();
    }
    return "";
}

std::string ReActLoop::reflectOnError(const ReActStep& failed_step) {
    std::ostringstream oss;
    oss << "上一步执行失败:\n";
    oss << "  尝试的操作: " << failed_step.action << "\n";
    oss << "  错误信息: " << failed_step.observation << "\n";
    oss << "请反思失败原因，调整方案后重试。如果是参数错误请修正参数，如果是工具不可用请换用其他方法。";
    return oss.str();
}

bool ReActLoop::executeToolCall(ReActStep& step) {
    if (step.action.empty()) {
        step.is_error = true;
        step.observation = "未指定操作名称";
        return false;
    }

    if (m_config.on_tool_call) {
        step.observation = m_config.on_tool_call(step.action, step.action_input);
        step.is_error = false;
        return !step.observation.empty();
    }

    auto& registry = ApiToolRegistry::instance();
    auto* tool = registry.getTool(step.action);
    if (!tool) {
        step.is_error = true;
        step.observation = "工具 '" + step.action + "' 未找到。可用工具: " +
            []() {
                std::string names;
                for (const auto& t : registry.allTools()) {
                    if (!names.empty()) names += ", ";
                    names += t.name;
                }
                return names;
            }();
        return false;
    }

    auto result = registry.executeTool(step.action, step.action_input);
    if (result.success) {
        step.observation = result.body;
        step.is_error = false;
        return true;
    } else {
        step.is_error = true;
        step.observation = result.body;
        return false;
    }
}

ReActResult ReActLoop::run(const std::string& task_description) {
    ReActResult result;

    std::string context = "任务: " + task_description;
    int reflection_retries = 0;

    for (int iter = 0; iter < m_config.max_iterations; ++iter) {
        ReActStep step;
        step.attempt = iter + 1;

        if (m_config.on_think) {
            step.thought = m_config.on_think(task_description, context);
        } else {
            step.thought = context;
        }

        step.action = parseActionName(step.thought);
        step.action_input = parseActionInput(step.thought);

        if (step.action.empty()) {
            result.final_answer = step.thought;
            result.success = true;
            result.total_attempts = iter + 1;
            result.trace.push_back(step);
            return result;
        }

        bool ok = executeToolCall(step);

        if (!ok && reflection_retries < m_config.max_reflection_retries) {
            std::string reflection = reflectOnError(step);
            result.trace.push_back(step);
            reflection_retries++;
            context = reflection + "\n\n" + context;
            if (m_config.verbose) {
                fprintf(stdout, "[ReAct] Step %d failed, retrying with reflection (%d/%d)\n",
                    iter + 1, reflection_retries, m_config.max_reflection_retries);
            }
            continue;
        }

        result.trace.push_back(step);
        reflection_retries = 0;

        if (ok) {
            context = "Observation: " + step.observation;
        } else {
            result.final_answer = "多次尝试失败: " + step.observation;
            result.success = false;
            result.error_message = step.observation;
            result.total_attempts = iter + 1;
            return result;
        }
    }

    if (!result.trace.empty()) {
        result.final_answer = result.trace.back().observation;
        result.success = !result.trace.back().is_error;
    }
    result.total_attempts = (int)result.trace.size();
    return result;
}

} // namespace npc
