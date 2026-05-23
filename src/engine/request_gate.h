#pragma once

#include <string>
#include <deque>
#include <functional>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <condition_variable>

namespace npc {

struct GateConfig {
    int max_concurrent = 3;
    int max_requests_per_minute = 60;
    int max_tokens_per_minute = 100000;
    int request_timeout_ms = 60000;
    int max_queue_size = 32;
    int retry_count = 2;
    int retry_delay_ms = 1000;
};

struct GateRequest {
    std::string id;
    std::string model_id;
    int64_t submitted_at = 0;
    std::function<void()> execute;
    bool cancelled = false;
};

struct GateStats {
    int64_t total_requests = 0;
    int64_t accepted = 0;
    int64_t rejected = 0;
    int64_t queued = 0;
    int64_t timed_out = 0;
    int current_queue = 0;
    int current_active = 0;
};

class RequestGate {
public:
    static RequestGate& instance();

    void configure(const std::string& modelId, const GateConfig& config);
    void configureDefault(const GateConfig& config);
    const GateConfig& getConfig(const std::string& modelId) const;

    bool submit(const std::string& modelId,
                 std::function<void()> execute,
                 const std::string& requestId = "");

    void cancel(const std::string& requestId);
    void cancelAll(const std::string& modelId);

    void processQueue(const std::string& modelId);

    void tick();

    GateStats stats(const std::string& modelId = "") const;

    size_t pendingCount(const std::string& modelId) const;
    int activeCount(const std::string& modelId) const;

private:
    RequestGate() = default;
    RequestGate(const RequestGate&) = delete;
    RequestGate& operator=(const RequestGate&) = delete;

    std::string generateId();
    int64_t nowMs() const;
    bool canAccept(const std::string& modelId) const;

    struct Slot {
        std::string request_id;
        int64_t started_at = 0;
    };
    struct ModelGate {
        GateConfig config;
        std::deque<GateRequest> queue;
        std::deque<Slot> active_slots;
        int64_t window_start = 0;
        int requests_in_window = 0;
        int tokens_in_window = 0;
        int next_id = 1;
    };

    GateConfig m_default_config;
    std::unordered_map<std::string, ModelGate> m_gates;
    GateStats m_total_stats;
    mutable std::mutex m_mutex;
};

} // namespace npc
