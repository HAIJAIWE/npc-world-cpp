#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <deque>
#include <functional>
#include <cstdint>

namespace npc {

enum class NodeType : uint8_t {
    LLM_GENERATE,
    TOOL_CALL,
    CONDITION,
    TRANSFORM,
    AGGREGATE,
    APPROVAL,
    DELAY,
    FILE_OP,
    WEB_FETCH,
    SCHEDULE_TASK,
    MONITOR
};

enum class NodeStatus : uint8_t {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    SKIPPED
};

struct WorkflowNode {
    std::string id;
    std::string name;
    NodeType type;
    NodeStatus status = NodeStatus::PENDING;

    std::vector<std::string> depends_on;
    std::vector<std::string> required_by;

    std::string prompt_template;
    std::string tool_name;
    std::string condition_expr;
    std::string transform_code;

    std::string output;
    std::string error;
    int64_t started_at = 0;
    int64_t finished_at = 0;
    int retry_count = 0;
    int max_retries = 1;
};

struct WorkflowDAG {
    std::string id;
    std::string name;
    std::vector<WorkflowNode> nodes;
    int64_t created_at = 0;
};

using NodeExecutorCallback = std::function<bool(WorkflowNode& node)>;

class WorkflowEngine {
public:
    static WorkflowEngine& instance();

    void registerExecutor(NodeType type, NodeExecutorCallback executor);

    WorkflowDAG buildDAG(const std::string& name, const std::vector<WorkflowNode>& nodes);
    bool executeDAG(WorkflowDAG& dag);

    std::string formatDAG(const WorkflowDAG& dag) const;
    std::string formatResult(const WorkflowDAG& dag) const;

    bool validateDAG(const WorkflowDAG& dag, std::string& error) const;

    struct EngineStats {
        int64_t dags_executed = 0;
        int64_t nodes_executed = 0;
        int64_t nodes_failed = 0;
    };
    EngineStats stats() const { return m_stats; }

private:
    WorkflowEngine() = default;

    std::unordered_map<NodeType, NodeExecutorCallback> m_executors;
    EngineStats m_stats;

    std::vector<WorkflowNode*> getReadyNodes(WorkflowDAG& dag);
    bool allDependenciesMet(const WorkflowNode& node, const WorkflowDAG& dag) const;
    std::vector<int> topologicalSort(const WorkflowDAG& dag, std::string& error) const;
    bool hasCycle(const WorkflowDAG& dag) const;
    bool dfs(int idx, std::vector<int>& visited, const std::vector<std::vector<int>>& adj) const;
};

} // namespace npc
