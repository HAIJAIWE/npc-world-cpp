#include "engine/inference_pipeline.h"
#include <chrono>

namespace npc {

InferencePipeline& InferencePipeline::instance() {
    static InferencePipeline pipeline;
    return pipeline;
}

void InferencePipeline::start(int32_t numWorkers) {
    if (m_running.load()) return;

    m_running.store(true);
    numWorkers = std::max(1, numWorkers);

    for (int32_t i = 0; i < numWorkers; i++) {
        m_workers.emplace_back(&InferencePipeline::workerLoop, this, i);
    }
}

void InferencePipeline::stop() {
    m_running.store(false);
    m_queueCV.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();
}

int64_t InferencePipeline::submit(const InferenceRequest& request, ResponseCallback callback) {
    int64_t reqId = request.id > 0 ? request.id : m_nextRequestId++;

    InferenceRequest req = request;
    req.id = reqId;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        if (static_cast<int32_t>(m_queue.size()) >= m_queueCapacity) {
            fprintf(stderr, "[InferencePipeline] Queue full, request %lld dropped\n", (long long)reqId);
            return -1;
        }

        m_queue.push(req);
    }

    if (callback) {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_callbacks.push_back({reqId, callback});
    }

    m_queueCV.notify_one();
    return reqId;
}

bool InferencePipeline::cancel(int64_t requestId) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    auto it = std::find_if(m_callbacks.begin(), m_callbacks.end(),
        [requestId](const CallbackEntry& e) { return e.requestId == requestId; });
    if (it != m_callbacks.end()) {
        m_callbacks.erase(it);
        return true;
    }
    return false;
}

size_t InferencePipeline::pendingCount() const {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_queue.size();
}

InferencePipeline::Stats InferencePipeline::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

InferenceResponse InferencePipeline::generateSync(const InferenceRequest& request,
                                                    StreamCallback streamCb) {
    auto& engine = ModelEngine::instance();

    ModelEngine::TokenCallback tokenCb = nullptr;
    if (streamCb) {
        tokenCb = [&streamCb](const std::string& text, bool done) -> bool {
            return streamCb(text, done);
        };
    }

    InferenceResponse response;
    response.requestId = request.id;
    response.success = true;

    try {
        GenerationResult genResult;
        if (request.useMessages) {
            response.text = engine.completionMessages(request.messages, &request.params, tokenCb);
            genResult.text = response.text;
        } else {
            response.text = engine.completion(request.systemPrompt, request.userPrompt,
                                              &request.params, tokenCb);
            genResult.text = response.text;
        }
        response.rawResult = genResult;

        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.totalRequests++;
            m_stats.totalTokens += genResult.n_tokens;
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error = e.what();
    }

    return response;
}

void InferencePipeline::workerLoop(int32_t workerId) {
    while (m_running.load()) {
        InferenceRequest request;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCV.wait(lock, [this] {
                return !m_queue.empty() || !m_running.load();
            });

            if (!m_running.load() && m_queue.empty()) break;

            request = m_queue.front();
            m_queue.pop();
        }

        m_activeCount++;

        auto response = processRequest(request);

        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            auto it = std::find_if(m_callbacks.begin(), m_callbacks.end(),
                [&request](const CallbackEntry& e) { return e.requestId == request.id; });
            if (it != m_callbacks.end()) {
                if (it->callback) {
                    it->callback(response);
                }
            }
        }

        m_activeCount--;
    }
}

InferenceResponse InferencePipeline::processRequest(const InferenceRequest& request) {
    auto& engine = ModelEngine::instance();

    InferenceResponse response;
    response.requestId = request.id;
    response.success = true;

    try {
        if (request.useMessages) {
            response.text = engine.completionMessages(request.messages, &request.params);
        } else {
            response.text = engine.completion(request.systemPrompt, request.userPrompt, &request.params);
        }
    } catch (const std::exception& e) {
        response.success = false;
        response.error = e.what();
    }

    return response;
}

} // namespace npc
