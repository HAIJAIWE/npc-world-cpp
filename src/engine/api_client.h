#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace npc {

struct ApiToolDef {
    std::string name;
    std::string description;
    std::string url;
    std::string method;
    std::string content_type;
    std::string body_template;
    int timeout_ms = 10000;
    bool enabled = true;
};

struct ApiCallResult {
    std::string tool_name;
    bool success;
    int status_code;
    std::string body;
    std::string error;
};

using ToolParamParser = std::function<std::string(const std::string& paramsJson,
                                                   const ApiToolDef& tool)>;

class ApiClient {
public:
    static ApiClient& instance();

    bool initialize();
    void shutdown();

    void setHeader(const std::string& key, const std::string& value);
    void clearHeaders();

    ApiCallResult call(const std::string& url, const std::string& method,
                        const std::string& body = "",
                        const std::string& contentType = "application/json",
                        int timeoutMs = 10000);

    ApiCallResult get(const std::string& url, int timeoutMs = 10000);
    ApiCallResult post(const std::string& url, const std::string& body,
                        const std::string& contentType = "application/json",
                        int timeoutMs = 10000);

private:
    ApiClient() = default;
    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;

    static std::string wideToUtf8(const wchar_t* wstr);
    static std::wstring utf8ToWide(const std::string& str);

    bool m_initialized = false;
    std::unordered_map<std::string, std::string> m_extra_headers;
    std::mutex m_mutex;
};

class ApiToolRegistry {
public:
    static ApiToolRegistry& instance();

    void registerTool(const ApiToolDef& tool);

    void registerTool(const std::string& name, const std::string& description,
                       const std::string& url, const std::string& method = "GET");

    void registerBuiltinTools();

    bool removeTool(const std::string& name);

    const ApiToolDef* getTool(const std::string& name) const;

    std::vector<ApiToolDef> allTools() const;

    std::string getToolsPrompt() const;

    bool parseToolCall(const std::string& text,
                        std::string& toolName,
                        std::string& params) const;

    ApiCallResult executeTool(const std::string& toolName,
                               const std::string& params);

    ApiCallResult executeWithRetry(const std::string& toolName,
                                    const std::string& params,
                                    int maxRetries = 2);

    std::string formatToolResult(const ApiCallResult& result) const;

    size_t toolCount() const;

private:
    ApiToolRegistry() = default;
    ApiToolRegistry(const ApiToolRegistry&) = delete;
    ApiToolRegistry& operator=(const ApiToolRegistry&) = delete;

    std::unordered_map<std::string, ApiToolDef> m_tools;
    mutable std::mutex m_mutex;
};

} // namespace npc
