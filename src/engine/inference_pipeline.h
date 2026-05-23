#pragma once

#include "engine/model_engine.h"
#include "engine/context_manager.h"
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace npc {

struct InferenceRequest {
    int64_t id;
    std::string systemPrompt;
    std::string userPrompt;
    std::vector<std::pair<std::string, std::string>> messages;
    ModelParams params;
    StopCriteria stopCriteria;
    bool useMessages = false;
};

struct InferenceResponse {
    int64_t requestId;
    std::string text;
    GenerationResult rawResult;
    bool success;
    std::string error;
};

class InferencePipeline {
public:
    using ResponseCallback = std::function<void(const InferenceResponse&)>;
    using StreamCallback = std::function<bool(const std::string& text, bool done)>;

    static InferencePipeline& instance();

    void start(int32_t numWorkers = 1);
    void stop();
    bool isRunning() const { return m_running.load(); }

    int64_t submit(const InferenceRequest& request, ResponseCallback callback = nullptr);
    bool cancel(int64_t requestId);

    void setBatchSize(int32_t batchSize) { m_batchSize = batchSize; }
    void setQueueCapacity(int32_t capacity) { m_queueCapacity = capacity; }

    size_t pendingCount() const;
    size_t activeCount() const { return m_activeCount.load(); }

    struct Stats {
        int64_t totalRequests;
        int64_t totalTokens;
        double avgTokensPerSec;
        double avgLatencyMs;
    };
    Stats getStats() const;

    InferenceResponse generateSync(const InferenceRequest& request,
                                    StreamCallback streamCb = nullptr);

private:
    InferencePipeline() = default;
    InferencePipeline(const InferencePipeline&) = delete;
    InferencePipeline& operator=(const InferencePipeline&) = delete;

    void workerLoop(int32_t workerId);
    InferenceResponse processRequest(const InferenceRequest& request);

    std::atomic<bool> m_running{false};
    std::atomic<size_t> m_activeCount{0};
    std::atomic<int64_t> m_nextRequestId{1};

    int32_t m_batchSize = 512;
    int32_t m_queueCapacity = 64;

    std::queue<InferenceRequest> m_queue;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCV;

    std::vector<std::thread> m_workers;

    struct CallbackEntry {
        int64_t requestId;
        ResponseCallback callback;
    };
    std::mutex m_callbackMutex;
    std::vector<CallbackEntry> m_callbacks;

    mutable std::mutex m_statsMutex;
    mutable Stats m_stats;
};

} // namespace npc
