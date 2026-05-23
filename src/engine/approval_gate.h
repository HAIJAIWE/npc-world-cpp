#pragma once
#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <chrono>
#include <cstdint>

namespace npc {

enum class RiskLevel : uint8_t {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2,
    CRITICAL = 3
};

struct ApprovalRequest {
    std::string id;
    std::string operation;
    std::string description;
    std::string params_json;
    RiskLevel risk = RiskLevel::LOW;
    int64_t created_at = 0;
    int timeout_seconds = 60;
};

struct ApprovalResponse {
    std::string request_id;
    bool approved = false;
    std::string reason;
};

class ApprovalGate {
public:
    static ApprovalGate& instance();

    void configure(int default_timeout_sec = 60);

    ApprovalRequest requestApproval(const std::string& operation,
                                     const std::string& description,
                                     const std::string& params_json = "",
                                     RiskLevel risk = RiskLevel::MEDIUM);

    bool isApproved(const std::string& request_id) const;
    void approve(const std::string& request_id, const std::string& reason = "");
    void deny(const std::string& request_id, const std::string& reason = "");

    void addToWhitelist(const std::string& operation);
    bool isWhitelisted(const std::string& operation) const;
    void clearWhitelist();

    RiskLevel classifyRisk(const std::string& operation) const;

    ApprovalRequest* getRequest(const std::string& request_id);
    std::vector<ApprovalRequest> pendingRequests() const;

    bool hasExpiredRequests();

    struct GateStats {
        int64_t total_requests = 0;
        int64_t approved = 0;
        int64_t denied = 0;
        int64_t expired = 0;
        int64_t whitelist_hits = 0;
    };
    GateStats stats() const { return m_stats; }

private:
    ApprovalGate() = default;

    std::deque<ApprovalRequest> m_pending;
    std::deque<ApprovalResponse> m_responses;
    std::vector<std::string> m_whitelist;
    int m_default_timeout = 60;
    int64_t m_next_id = 1;
    GateStats m_stats;

    int64_t now() const;
    std::string generateId();
};

} // namespace npc
