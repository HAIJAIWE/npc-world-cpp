#include "engine/approval_gate.h"
#include <sstream>
#include <algorithm>
#include <ctime>

namespace npc {

ApprovalGate& ApprovalGate::instance() {
    static ApprovalGate inst;
    return inst;
}

void ApprovalGate::configure(int default_timeout_sec) {
    m_default_timeout = default_timeout_sec;
}

int64_t ApprovalGate::now() const {
    return (int64_t)time(nullptr);
}

std::string ApprovalGate::generateId() {
    return "approval_" + std::to_string(m_next_id++);
}

RiskLevel ApprovalGate::classifyRisk(const std::string& operation) const {
    std::string lower = operation;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });

    if (lower.find("delete") != std::string::npos ||
        lower.find("remove") != std::string::npos ||
        lower.find("destroy") != std::string::npos ||
        lower.find("drop") != std::string::npos) {
        return RiskLevel::CRITICAL;
    }

    if (lower.find("save") != std::string::npos ||
        lower.find("write") != std::string::npos ||
        lower.find("modify") != std::string::npos ||
        lower.find("update") != std::string::npos) {
        return RiskLevel::HIGH;
    }

    if (lower.find("create") != std::string::npos ||
        lower.find("send") != std::string::npos ||
        lower.find("publish") != std::string::npos) {
        return RiskLevel::MEDIUM;
    }

    return RiskLevel::LOW;
}

ApprovalRequest ApprovalGate::requestApproval(const std::string& operation,
                                                const std::string& description,
                                                const std::string& params_json,
                                                RiskLevel risk) {
    if (risk == RiskLevel::LOW) {
        m_stats.whitelist_hits++;
        m_stats.approved++;
        ApprovalRequest req;
        req.id = generateId();
        req.operation = operation;
        return req;
    }

    if (isWhitelisted(operation)) {
        m_stats.whitelist_hits++;
        m_stats.approved++;
        ApprovalRequest req;
        req.id = generateId();
        req.operation = operation;
        return req;
    }

    if (risk == RiskLevel::LOW) risk = classifyRisk(operation);

    ApprovalRequest req;
    req.id = generateId();
    req.operation = operation;
    req.description = description;
    req.params_json = params_json;
    req.risk = risk;
    req.created_at = now();
    req.timeout_seconds = m_default_timeout;

    m_pending.push_back(req);
    m_stats.total_requests++;
    return req;
}

bool ApprovalGate::isApproved(const std::string& request_id) const {
    for (const auto& resp : m_responses) {
        if (resp.request_id == request_id) return resp.approved;
    }
    return false;
}

void ApprovalGate::approve(const std::string& request_id, const std::string& reason) {
    m_responses.push_back({request_id, true, reason});
    m_stats.approved++;

    m_pending.erase(
        std::remove_if(m_pending.begin(), m_pending.end(),
            [&](const ApprovalRequest& r) { return r.id == request_id; }),
        m_pending.end());
}

void ApprovalGate::deny(const std::string& request_id, const std::string& reason) {
    m_responses.push_back({request_id, false, reason});
    m_stats.denied++;

    m_pending.erase(
        std::remove_if(m_pending.begin(), m_pending.end(),
            [&](const ApprovalRequest& r) { return r.id == request_id; }),
        m_pending.end());
}

void ApprovalGate::addToWhitelist(const std::string& operation) {
    if (!isWhitelisted(operation)) {
        m_whitelist.push_back(operation);
    }
}

bool ApprovalGate::isWhitelisted(const std::string& operation) const {
    return std::find(m_whitelist.begin(), m_whitelist.end(), operation) != m_whitelist.end();
}

void ApprovalGate::clearWhitelist() {
    m_whitelist.clear();
}

ApprovalRequest* ApprovalGate::getRequest(const std::string& request_id) {
    for (auto& req : m_pending) {
        if (req.id == request_id) return &req;
    }
    return nullptr;
}

std::vector<ApprovalRequest> ApprovalGate::pendingRequests() const {
    return {m_pending.begin(), m_pending.end()};
}

bool ApprovalGate::hasExpiredRequests() {
    int64_t n = now();
    bool expired = false;
    auto it = m_pending.begin();
    while (it != m_pending.end()) {
        if (n - it->created_at > it->timeout_seconds) {
            m_stats.expired++;
            it = m_pending.erase(it);
            expired = true;
        } else {
            ++it;
        }
    }
    return expired;
}

} // namespace npc
