#include "engine/workflow_engine.h"
#include <sstream>
#include <queue>
#include <algorithm>
#include <ctime>

namespace npc {

WorkflowEngine& WorkflowEngine::instance() {
    static WorkflowEngine inst;
    return inst;
}

void WorkflowEngine::registerExecutor(NodeType type, NodeExecutorCallback executor) {
    m_executors[type] = std::move(executor);
}

WorkflowDAG WorkflowEngine::buildDAG(const std::string& name,
                                       const std::vector<WorkflowNode>& nodes) {
    WorkflowDAG dag;
    dag.id = "wf_" + std::to_string((int64_t)time(nullptr));
    dag.name = name;
    dag.nodes = nodes;
    dag.created_at = (int64_t)time(nullptr);

    for (auto& node : dag.nodes) {
        for (const auto& dep_id : node.depends_on) {
            for (auto& other : dag.nodes) {
                if (other.id == dep_id) {
                    if (std::find(other.required_by.begin(), other.required_by.end(), node.id) == other.required_by.end()) {
                        other.required_by.push_back(node.id);
                    }
                }
            }
        }
    }

    return dag;
}

bool WorkflowEngine::hasCycle(const WorkflowDAG& dag) const {
    int n = (int)dag.nodes.size();
    std::vector<std::vector<int>> adj(n);
    for (int i = 0; i < n; ++i) {
        for (const auto& dep : dag.nodes[i].depends_on) {
            for (int j = 0; j < n; ++j) {
                if (dag.nodes[j].id == dep) {
                    adj[i].push_back(j);
                }
            }
        }
    }

    std::vector<int> visited(n, 0);
    for (int i = 0; i < n; ++i) {
        if (!visited[i] && dfs(i, visited, adj)) return true;
    }
    return false;
}

bool WorkflowEngine::dfs(int idx, std::vector<int>& visited,
                          const std::vector<std::vector<int>>& adj) const {
    visited[idx] = 1;
    for (int next : adj[idx]) {
        if (visited[next] == 1) return true;
        if (visited[next] == 0 && dfs(next, visited, adj)) return true;
    }
    visited[idx] = 2;
    return false;
}

bool WorkflowEngine::validateDAG(const WorkflowDAG& dag, std::string& error) const {
    if (dag.nodes.empty()) {
        error = "DAG 没有节点";
        return false;
    }

    std::set<std::string> ids;
    for (const auto& node : dag.nodes) {
        if (ids.count(node.id)) {
            error = "重复的节点ID: " + node.id;
            return false;
        }
        ids.insert(node.id);
    }

    for (const auto& node : dag.nodes) {
        for (const auto& dep : node.depends_on) {
            if (!ids.count(dep)) {
                error = "节点 " + node.id + " 引用了不存在的依赖: " + dep;
                return false;
            }
        }
    }

    if (hasCycle(dag)) {
        error = "DAG 存在循环依赖";
        return false;
    }

    return true;
}

std::vector<int> WorkflowEngine::topologicalSort(const WorkflowDAG& dag,
                                                    std::string& error) const {
    int n = (int)dag.nodes.size();
    std::vector<std::vector<int>> adj(n);
    std::vector<int> indegree(n, 0);

    for (int i = 0; i < n; ++i) {
        for (const auto& dep : dag.nodes[i].depends_on) {
            for (int j = 0; j < n; ++j) {
                if (dag.nodes[j].id == dep) {
                    adj[j].push_back(i);
                    indegree[i]++;
                    break;
                }
            }
        }
    }

    std::queue<int> q;
    for (int i = 0; i < n; ++i) {
        if (indegree[i] == 0) q.push(i);
    }

    std::vector<int> order;
    while (!q.empty()) {
        int u = q.front(); q.pop();
        order.push_back(u);
        for (int v : adj[u]) {
            if (--indegree[v] == 0) q.push(v);
        }
    }

    if ((int)order.size() != n) {
        error = "拓扑排序失败: 可能存在循环依赖";
        return {};
    }

    return order;
}

bool WorkflowEngine::allDependenciesMet(const WorkflowNode& node, const WorkflowDAG& dag) const {
    for (const auto& dep_id : node.depends_on) {
        bool met = false;
        for (const auto& other : dag.nodes) {
            if (other.id == dep_id && other.status == NodeStatus::COMPLETED) {
                met = true;
                break;
            }
        }
        if (!met) return false;
    }
    return true;
}

std::vector<WorkflowNode*> WorkflowEngine::getReadyNodes(WorkflowDAG& dag) {
    std::vector<WorkflowNode*> ready;
    for (auto& node : dag.nodes) {
        if (node.status == NodeStatus::PENDING && allDependenciesMet(node, dag)) {
            ready.push_back(&node);
        }
    }
    return ready;
}

bool WorkflowEngine::executeDAG(WorkflowDAG& dag) {
    std::string validate_err;
    if (!validateDAG(dag, validate_err)) {
        return false;
    }

    std::string sort_err;
    auto order = topologicalSort(dag, sort_err);
    if (order.empty()) return false;

    bool all_ok = true;
    for (int idx : order) {
        auto& node = dag.nodes[idx];
        node.status = NodeStatus::RUNNING;
        node.started_at = (int64_t)time(nullptr);

        auto exec_it = m_executors.find(node.type);
        if (exec_it != m_executors.end() && exec_it->second) {
            bool ok = false;
            int retries = 0;
            while (retries <= node.max_retries) {
                ok = exec_it->second(node);
                if (ok) break;
                retries++;
                node.retry_count = retries;
            }
            if (ok) {
                node.status = NodeStatus::COMPLETED;
                m_stats.nodes_executed++;
            } else {
                node.status = NodeStatus::FAILED;
                node.error = "执行失败 (重试 " + std::to_string(retries) + " 次)";
                m_stats.nodes_failed++;
                all_ok = false;
            }
        } else {
            node.status = NodeStatus::COMPLETED;
            node.output = "(无执行器, 跳过)";
            m_stats.nodes_executed++;
        }

        node.finished_at = (int64_t)time(nullptr);
    }

    m_stats.dags_executed++;
    return all_ok;
}

std::string WorkflowEngine::formatDAG(const WorkflowDAG& dag) const {
    std::ostringstream oss;
    oss << "Workflow: " << dag.name << " (" << dag.nodes.size() << " nodes)\n";
    for (const auto& node : dag.nodes) {
        const char* status_str = "?";
        switch (node.status) {
            case NodeStatus::PENDING:   status_str = "PENDING"; break;
            case NodeStatus::RUNNING:   status_str = "RUNNING"; break;
            case NodeStatus::COMPLETED: status_str = "OK"; break;
            case NodeStatus::FAILED:    status_str = "FAILED"; break;
            case NodeStatus::SKIPPED:   status_str = "SKIPPED"; break;
        }
        oss << "  [" << status_str << "] " << node.name << " (" << node.id << ")";
        if (!node.depends_on.empty()) {
            oss << " ← ";
            for (size_t i = 0; i < node.depends_on.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << node.depends_on[i];
            }
        }
        oss << "\n";
    }
    return oss.str();
}

std::string WorkflowEngine::formatResult(const WorkflowDAG& dag) const {
    std::ostringstream oss;
    for (const auto& node : dag.nodes) {
        if (node.status == NodeStatus::COMPLETED && !node.output.empty()) {
            oss << "[" << node.name << "]\n" << node.output << "\n\n";
        } else if (node.status == NodeStatus::FAILED) {
            oss << "[" << node.name << " FAILED] " << node.error << "\n";
        }
    }
    return oss.str();
}

} // namespace npc