#include "engine/api_model_backend.h"
#include "engine/api_client.h"
#include <chrono>
#include <sstream>
#include <random>

namespace npc {

ApiModelBackend& ApiModelBackend::instance() {
    static ApiModelBackend backend;
    return backend;
}

void ApiModelBackend::addProvider(const std::string& name, const ApiModelConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_providers[name] = config;
    m_stats[name] = {};
    fprintf(stdout, "[ApiModel] Provider '%s' added: %s @ %s\n",
        name.c_str(), config.model_name.c_str(), config.api_url.c_str());
}

bool ApiModelBackend::removeProvider(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.erase(name);
    return m_providers.erase(name) > 0;
}

const ApiModelConfig* ApiModelBackend::getProvider(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_providers.find(name);
    return it != m_providers.end() ? &it->second : nullptr;
}

std::vector<std::string> ApiModelBackend::providerNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_providers.size());
    for (const auto& [name, _] : m_providers) {
        names.push_back(name);
    }
    return names;
}

bool ApiModelBackend::providerReady(const std::string& name) const {
    auto* cfg = getProvider(name);
    return cfg && cfg->valid();
}

std::string ApiModelBackend::buildOpenAiBody(
    const std::vector<std::pair<std::string, std::string>>& messages,
    const ApiModelConfig& config,
    const std::string& systemPrompt) const {

    std::ostringstream oss;
    oss << "{";
    oss << "\"model\":\"" << config.model_name << "\",";
    oss << "\"messages\":[";

    bool first = true;
    if (!systemPrompt.empty()) {
        oss << "{\"role\":\"system\",\"content\":"  << "\"" << systemPrompt << "\"}";
        first = false;
    }

    for (const auto& [role, content] : messages) {
        if (!first) oss << ",";
        std::string role_name = role;
        if (role_name == "user" || role_name == "assistant" || role_name == "system") {
        } else {
            role_name = "user";
        }
        std::string escaped = content;
        for (size_t i = 0; i < escaped.size();) {
            if (escaped[i] == '"') { escaped.replace(i, 1, "\\\""); i += 2; }
            else if (escaped[i] == '\\') { escaped.replace(i, 1, "\\\\"); i += 2; }
            else if (escaped[i] == '\n') { escaped.replace(i, 1, "\\n"); i += 2; }
            else if (escaped[i] == '\r') { escaped.erase(i, 1); }
            else if (escaped[i] == '\t') { escaped.replace(i, 1, "\\t"); i += 2; }
            else { ++i; }
        }
        oss << "{\"role\":\"" << role_name << "\",\"content\":\"" << escaped << "\"}";
        first = false;
    }
    oss << "],";

    oss << "\"temperature\":" << config.temperature << ",";
    oss << "\"top_p\":" << config.top_p << ",";
    oss << "\"max_tokens\":" << config.max_tokens;
    oss << "}";

    return oss.str();
}

std::string ApiModelBackend::parseOpenAiResponse(const std::string& body,
                                                  ApiModelResult& result) const {
    std::string content;
    auto extract = [](const std::string& json, const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();

        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n'))
            pos++;

        if (pos >= json.size()) return "";

        if (json[pos] == '"') {
            pos++;
            std::string val;
            while (pos < json.size()) {
                if (json[pos] == '\\' && pos + 1 < json.size()) {
                    char next = json[pos + 1];
                    if (next == '"') val += '"';
                    else if (next == 'n') val += '\n';
                    else if (next == '\\') val += '\\';
                    else if (next == 't') val += '\t';
                    else val += json[pos + 1];
                    pos += 2;
                } else if (json[pos] == '"') {
                    break;
                } else {
                    val += json[pos];
                    pos++;
                }
            }
            return val;
        }
        if (json[pos] >= '0' && json[pos] <= '9') {
            std::string val;
            while (pos < json.size() && ((json[pos] >= '0' && json[pos] <= '9') || json[pos] == '.')) {
                val += json[pos++];
            }
            return val;
        }
        return "";
    };

    auto extractInt = [&extract](const std::string& json, const std::string& key) -> int {
        auto val = extract(json, key);
        if (val.empty()) return 0;
        try { return std::stoi(val); } catch (...) { return 0; }
    };

    auto extractStr = [&extract](const std::string& json, const std::string& key) -> std::string {
        return extract(json, key);
    };

    content = extractStr(body, "content");

    if (content.empty()) {
        size_t msgPos = body.find("\"message\"");
        if (msgPos != std::string::npos) {
            content = extractStr(body.substr(msgPos), "content");
        }
    }

    result.prompt_tokens = extractInt(body, "prompt_tokens");
    result.completion_tokens = extractInt(body, "completion_tokens");

    return content;
}

std::string ApiModelBackend::parseStreamChunk(const std::string& chunk) const {
    std::string text;
    size_t pos = 0;
    while ((pos = chunk.find("\"delta\":{\"content\":\"", pos)) != std::string::npos) {
        pos += 20;
        while (pos < chunk.size() && chunk[pos] != '"') {
            if (chunk[pos] == '\\' && pos + 1 < chunk.size()) {
                pos += 2;
            } else {
                text += chunk[pos++];
            }
        }
        pos++;
    }
    return text;
}

ApiModelResult ApiModelBackend::chat(const std::string& provider,
                                      const std::vector<std::pair<std::string, std::string>>& messages,
                                      const std::string& systemPrompt) {
    ApiModelResult result;

    auto* cfg = getProvider(provider);
    if (!cfg || !cfg->valid()) {
        result.error = "Provider not found or invalid: " + provider;
        return result;
    }

    auto start = std::chrono::steady_clock::now();

    std::string body = buildOpenAiBody(messages, *cfg, systemPrompt);

    auto& client = ApiClient::instance();
    client.initialize();
    client.setHeader("Authorization", "Bearer " + cfg->api_key);

    auto raw = client.post(cfg->api_url, body, "application/json", cfg->timeout_ms);

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    result.status_code = raw.status_code;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& s = m_stats[provider];
        s.total_requests++;
    }

    if (!raw.success) {
        result.error = raw.error.empty() ? "HTTP " + std::to_string(raw.status_code) : raw.error;
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& s = m_stats[provider];
        s.error_count++;
        s.last_error = result.error;
        return result;
    }

    result.text = parseOpenAiResponse(raw.body, result);

    if (result.text.empty() && !raw.body.empty()) {
        result.text = "API Response:\n" + raw.body.substr(0, 500);
    }

    result.success = !result.text.empty();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& s = m_stats[provider];
        if (result.success) {
            s.success_count++;
            s.total_prompt_tokens += result.prompt_tokens;
            s.total_completion_tokens += result.completion_tokens;
            s.total_elapsed_ms += result.elapsed_ms;
        } else {
            s.error_count++;
            s.last_error = "Empty response from API";
        }
    }

    if (result.elapsed_ms > 0 && result.completion_tokens > 0) {
        result.tokens_per_second = result.completion_tokens * 1000.0 / result.elapsed_ms;
    }

    return result;
}

ApiModelResult ApiModelBackend::chatStream(const std::string& provider,
                                            const std::vector<std::pair<std::string, std::string>>& messages,
                                            ApiStreamCallback callback,
                                            const std::string& systemPrompt) {
    ApiModelResult result;
    result = chat(provider, messages, systemPrompt);
    if (callback && result.success) {
        callback(result.text, true);
    }
    return result;
}

ApiModelStats ApiModelBackend::stats(const std::string& provider) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stats.find(provider);
    if (it != m_stats.end()) return it->second;
    return {};
}

ApiModelStats ApiModelBackend::totalStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    ApiModelStats total;
    for (const auto& [_, s] : m_stats) {
        total.total_requests += s.total_requests;
        total.success_count += s.success_count;
        total.error_count += s.error_count;
        total.total_prompt_tokens += s.total_prompt_tokens;
        total.total_completion_tokens += s.total_completion_tokens;
        total.total_elapsed_ms += s.total_elapsed_ms;
    }
    return total;
}

} // namespace npc
