#include "engine/request_gate.h"
#include <chrono>
#include <random>

namespace npc {

RequestGate& RequestGate::instance() {
    static RequestGate gate;
    return gate;
}

void RequestGate::configure(const std::string& modelId, const GateConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    ModelGate gate;
    gate.config = config;
    m_gates[modelId] = gate;
    m_gates[modelId].next_id = 1;

    fprintf(stdout, "[RequestGate] %s configured: concurrent=%d, rpm=%d, tpm=%d, queue=%d\n",
        modelId.c_str(), config.max_concurrent, config.max_requests_per_minute,
        config.max_tokens_per_minute, config.max_queue_size);
}

void RequestGate::configureDefault(const GateConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_default_config = config;
}

const GateConfig& RequestGate::getConfig(const std::string& modelId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_gates.find(modelId);
    return it != m_gates.end() ? it->second.config : m_default_config;
}

std::string RequestGate::generateId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    return "gate_" + std::to_string(dis(gen));
}

int64_t RequestGate::nowMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool RequestGate::canAccept(const std::string& modelId) const {
    auto& gate = const_cast<std::unordered_map<std::string, ModelGate>&>(m_gates)[modelId];
    const auto& cfg = gate.config;

    if ((int)gate.active_slots.size() >= cfg.max_concurrent) {
        return false;
    }

    int64_t now = nowMs();
    if (now - gate.window_start > 60000) {
        gate.window_start = now;
        gate.requests_in_window = 0;
        gate.tokens_in_window = 0;
    }

    if (gate.requests_in_window >= cfg.max_requests_per_minute) {
        return false;
    }

    return true;
}

bool RequestGate::submit(const std::string& modelId,
                          std::function<void()> execute,
                          const std::string& requestId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_total_stats.total_requests++;

    auto gateIt = m_gates.find(modelId);
    if (gateIt == m_gates.end()) {
        GateConfig defaultCfg = m_default_config;
        defaultCfg.max_concurrent = 1;
        defaultCfg.max_requests_per_minute = 10;
        configure(modelId, defaultCfg);
        gateIt = m_gates.find(modelId);
    }

    auto& gate = gateIt->second;

    if (canAccept(modelId)) {
        Slot slot;
        slot.started_at = nowMs();
        slot.request_id = requestId.empty() ? generateId() : requestId;
        gate.active_slots.push_back(slot);
        gate.requests_in_window++;

        m_total_stats.accepted++;
        m_total_stats.current_active = (int)gate.active_slots.size();

        auto exec = execute;
        std::string rid = slot.request_id;
        std::thread worker([exec, this, modelId, rid]() {
            exec();
            std::lock_guard<std::mutex> lock2(m_mutex);
            auto git = m_gates.find(modelId);
            if (git != m_gates.end()) {
                auto& g = git->second;
                for (auto it = g.active_slots.begin(); it != g.active_slots.end(); ++it) {
                    if (it->request_id == rid) {
                        g.active_slots.erase(it);
                        break;
                    }
                }
                m_total_stats.current_active = (int)g.active_slots.size();
            }
            processQueue(modelId);
        });
        worker.detach();

        fprintf(stdout, "[RequestGate] %s accepted request %s (queue=%zu, active=%zu)\n",
            modelId.c_str(), rid.c_str(), gate.queue.size(), gate.active_slots.size());
        return true;
    }

    if ((int)gate.queue.size() >= gate.config.max_queue_size) {
        m_total_stats.rejected++;
        fprintf(stdout, "[RequestGate] %s rejected - queue full\n", modelId.c_str());
        return false;
    }

    GateRequest gReq;
    gReq.id = requestId.empty() ? generateId() : requestId;
    gReq.model_id = modelId;
    gReq.submitted_at = nowMs();
    gReq.execute = std::move(execute);
    gate.queue.push_back(std::move(gReq));

    m_total_stats.queued++;
    m_total_stats.current_queue = (int)gate.queue.size();
    fprintf(stdout, "[RequestGate] %s queued request %s (pos=%zu)\n",
        modelId.c_str(), gReq.id.c_str(), gate.queue.size());
    return true;
}

void RequestGate::cancel(const std::string& requestId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [_, gate] : m_gates) {
        for (auto& req : gate.queue) {
            if (req.id == requestId) {
                req.cancelled = true;
                return;
            }
        }
    }
}

void RequestGate::cancelAll(const std::string& modelId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_gates.find(modelId);
    if (it != m_gates.end()) {
        for (auto& req : it->second.queue) {
            req.cancelled = true;
        }
    }
}

void RequestGate::processQueue(const std::string& modelId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_gates.find(modelId);
    if (it == m_gates.end()) return;

    auto& gate = it->second;
    int64_t now = nowMs();

    while (!gate.queue.empty() && canAccept(modelId)) {
        auto& req = gate.queue.front();
        if (req.cancelled) {
            gate.queue.pop_front();
            continue;
        }

        if (now - req.submitted_at > gate.config.request_timeout_ms) {
            m_total_stats.timed_out++;
            gate.queue.pop_front();
            continue;
        }

        Slot slot;
        slot.started_at = now;
        slot.request_id = req.id;
        gate.active_slots.push_back(slot);
        gate.requests_in_window++;

        auto exec = std::move(req.execute);
        std::string rid = req.id;
        std::string mid = modelId;

        std::thread worker([exec, this, mid, rid]() {
            exec();
            std::lock_guard<std::mutex> lock2(m_mutex);
            auto git = m_gates.find(mid);
            if (git != m_gates.end()) {
                auto& g = git->second;
                for (auto it2 = g.active_slots.begin(); it2 != g.active_slots.end(); ++it2) {
                    if (it2->request_id == rid) {
                        g.active_slots.erase(it2);
                        break;
                    }
                }
                m_total_stats.current_active = (int)g.active_slots.size();
            }
            processQueue(mid);
        });
        worker.detach();

        gate.queue.pop_front();
    }

    m_total_stats.current_queue = (int)gate.queue.size();
}

void RequestGate::tick() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [modelId, gate] : m_gates) {
        processQueue(modelId);
    }
}

GateStats RequestGate::stats(const std::string& modelId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (modelId.empty()) {
        GateStats s = m_total_stats;
        for (const auto& [_, gate] : m_gates) {
            s.current_queue += (int)gate.queue.size();
            s.current_active += (int)gate.active_slots.size();
        }
        return s;
    }
    auto it = m_gates.find(modelId);
    if (it == m_gates.end()) return {};

    GateStats s;
    s.current_queue = (int)it->second.queue.size();
    s.current_active = (int)it->second.active_slots.size();
    return s;
}

size_t RequestGate::pendingCount(const std::string& modelId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_gates.find(modelId);
    return it != m_gates.end() ? it->second.queue.size() : 0;
}

int RequestGate::activeCount(const std::string& modelId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_gates.find(modelId);
    return it != m_gates.end() ? (int)it->second.active_slots.size() : 0;
}

} // namespace npc