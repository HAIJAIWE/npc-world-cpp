#pragma once
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include "engine/api_client.h"

namespace npc {

struct ReActStep {
    std::string thought;
    std::string action;
    std::string action_input;
    std::string observation;
    bool is_error = false;
    int attempt = 0;
};

struct ReActResult {
    std::string final_answer;
    std::vector<ReActStep> trace;
    bool success = false;
    int total_attempts = 0;
    std::string error_message;

    std::string format_trace() const;
};

using ThoughtCallback   = std::function<std::string(const std::string& question, const std::string& context)>;
using ToolResultCallback = std::function<std::string(const std::string& tool_name, const std::string& params)>;

struct ReActConfig {
    int max_iterations = 5;
    int max_reflection_retries = 2;
    bool verbose = false;
    ThoughtCallback on_think;
    ToolResultCallback on_tool_call;
};

class ReActLoop {
public:
    static ReActLoop& instance();

    void configure(const ReActConfig& cfg);
    ReActResult run(const std::string& task_description);

    const ReActConfig& config() const { return m_config; }

private:
    ReActLoop();
    ReActConfig m_config;

    bool executeToolCall(ReActStep& step);
    std::string reflectOnError(const ReActStep& failed_step);
};

} // namespace npc
