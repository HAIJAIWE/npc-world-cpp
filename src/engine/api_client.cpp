#include "engine/api_client.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace npc {

std::wstring ApiClient::utf8ToWide(const std::string& str) {
    if (str.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], len);
    return result;
}

std::string ApiClient::wideToUtf8(const wchar_t* wstr) {
    if (!wstr || !*wstr) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], len, nullptr, nullptr);
    return result;
}

ApiClient& ApiClient::instance() {
    static ApiClient client;
    return client;
}

bool ApiClient::initialize() {
    if (m_initialized) return true;
    m_initialized = true;
    fprintf(stdout, "[ApiClient] WinHTTP initialized\n");
    return true;
}

void ApiClient::shutdown() {
    m_initialized = false;
}

void ApiClient::setHeader(const std::string& key, const std::string& value) {
    m_extra_headers[key] = value;
}

void ApiClient::clearHeaders() {
    m_extra_headers.clear();
}

ApiCallResult ApiClient::call(const std::string& url, const std::string& method,
                               const std::string& body, const std::string& contentType,
                               int timeoutMs) {
    ApiCallResult result;
    result.success = false;

    std::wstring wUrl = utf8ToWide(url);
    std::wstring wMethod = utf8ToWide(method);

    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostName[256] = {0};
    wchar_t urlPath[2048] = {0};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        result.error = "Failed to parse URL";
        return result;
    }

    HINTERNET hSession = WinHttpOpen(L"NPC-World-ApiClient/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        result.error = "Failed to open WinHTTP session";
        return result;
    }

    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    HINTERNET hConnect = WinHttpConnect(hSession, hostName,
        urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        result.error = "Failed to connect";
        return result;
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS)
        ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
        wMethod.c_str(), urlPath, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        result.error = "Failed to open request";
        return result;
    }

    BOOL sendResult = FALSE;
    if (!body.empty()) {
        std::string headers = "Content-Type: " + contentType + "\r\n";
        for (const auto& [k, v] : m_extra_headers) {
            headers += k + ": " + v + "\r\n";
        }
        std::wstring wHeaders = utf8ToWide(headers);
        std::string fullBody = body;
        sendResult = WinHttpSendRequest(hRequest, wHeaders.c_str(),
            (DWORD)wHeaders.size(), (LPVOID)fullBody.c_str(),
            (DWORD)fullBody.size(), (DWORD)fullBody.size(), 0);
    } else {
        std::string headers;
        for (const auto& [k, v] : m_extra_headers) {
            headers += k + ": " + v + "\r\n";
        }
        std::wstring wHeaders;
        LPCWSTR pwHeaders = WINHTTP_NO_ADDITIONAL_HEADERS;
        DWORD headerLen = 0;
        if (!headers.empty()) {
            wHeaders = utf8ToWide(headers);
            pwHeaders = wHeaders.c_str();
            headerLen = (DWORD)wHeaders.size();
        }
        sendResult = WinHttpSendRequest(hRequest, pwHeaders,
            headerLen, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    }

    if (!sendResult) {
        result.error = "Failed to send request (err=" +
            std::to_string(GetLastError()) + ")";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        result.error = "Failed to receive response";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
        WINHTTP_NO_HEADER_INDEX);
    result.status_code = (int)statusCode;

    std::string responseBody;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            responseBody.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    result.body = responseBody;
    result.success = (statusCode >= 200 && statusCode < 300);
    if (!result.success) {
        result.error = "HTTP " + std::to_string(statusCode);
    }
    return result;
}

ApiCallResult ApiClient::get(const std::string& url, int timeoutMs) {
    return call(url, "GET", "", "", timeoutMs);
}

ApiCallResult ApiClient::post(const std::string& url, const std::string& body,
                               const std::string& contentType, int timeoutMs) {
    return call(url, "POST", body, contentType, timeoutMs);
}

ApiToolRegistry& ApiToolRegistry::instance() {
    static ApiToolRegistry registry;
    return registry;
}

void ApiToolRegistry::registerTool(const ApiToolDef& tool) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tools[tool.name] = tool;
    fprintf(stdout, "[ApiTool] Registered tool '%s': %s %s\n",
        tool.name.c_str(), tool.method.c_str(), tool.url.c_str());
}

void ApiToolRegistry::registerTool(const std::string& name,
                                    const std::string& description,
                                    const std::string& url,
                                    const std::string& method) {
    ApiToolDef tool;
    tool.name = name;
    tool.description = description;
    tool.url = url;
    tool.method = method;
    registerTool(tool);
}

void ApiToolRegistry::registerBuiltinTools() {
    registerTool("get_time", "获取当前游戏世界时间", "world://time", "GET");
    registerTool("get_weather", "获取指定地点的天气状况", "world://weather", "GET");
    registerTool("get_location_info", "获取指定地点的详细信息", "world://location", "GET");
    registerTool("get_npc_info", "获取指定NPC的公开信息", "world://npc_info", "GET");
    registerTool("search_knowledge", "搜索世界知识库中的信息", "world://knowledge", "GET");
    registerTool("get_relationships", "获取NPC之间的关系网络", "world://relationships", "GET");
    fprintf(stdout, "[ApiTool] Registered %zu built-in tools\n", m_tools.size());
}

bool ApiToolRegistry::removeTool(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tools.erase(name) > 0;
}

const ApiToolDef* ApiToolRegistry::getTool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tools.find(name);
    return it != m_tools.end() ? &it->second : nullptr;
}

std::vector<ApiToolDef> ApiToolRegistry::allTools() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ApiToolDef> result;
    result.reserve(m_tools.size());
    for (const auto& [name, tool] : m_tools) {
        result.push_back(tool);
    }
    return result;
}

std::string ApiToolRegistry::getToolsPrompt() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_tools.empty()) return "";

    std::ostringstream oss;
    oss << "你可以使用以下工具来获取信息。"
        << "请用以下JSON格式输出：\n"
        << "{\"tool_call\": \"工具名称\", \"params\": \"参数\"}\n\n"
        << "可用工具列表：\n";

    for (const auto& [name, tool] : m_tools) {
        if (!tool.enabled) continue;
        oss << "- **" << name << "**: " << tool.description << "\n";
    }

    return oss.str();
}

bool ApiToolRegistry::parseToolCall(const std::string& text,
                                     std::string& toolName,
                                     std::string& params) const {
    auto extractJsonValue = [](const std::string& json, const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) {
            search = "\"" + key + "\": \"";
            pos = json.find(search);
        }
        if (pos == std::string::npos) return "";
        pos += search.size();
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };

    size_t tcPos = text.find("\"tool_call\"");
    if (tcPos != std::string::npos) {
        toolName = extractJsonValue(text, "tool_call");
        params = extractJsonValue(text, "params");
        return !toolName.empty();
    }

    tcPos = text.find("<tool_call>");
    if (tcPos != std::string::npos) {
        size_t tcEnd = text.find("</tool_call>", tcPos);
        if (tcEnd != std::string::npos) {
            toolName = text.substr(tcPos + 11, tcEnd - tcPos - 11);
            size_t pPos = text.find("<params>");
            size_t pEnd = text.find("</params>");
            if (pPos != std::string::npos && pEnd != std::string::npos) {
                params = text.substr(pPos + 8, pEnd - pPos - 8);
            }
            return !toolName.empty();
        }
    }

    return false;
}

ApiCallResult ApiToolRegistry::executeTool(const std::string& toolName,
                                            const std::string& params) {
    ApiCallResult result;
    result.tool_name = toolName;
    result.success = false;

    const ApiToolDef* tool = getTool(toolName);
    if (!tool) {
        result.error = "Unknown tool: " + toolName;
        return result;
    }

    fprintf(stdout, "[ApiTool] Executing '%s' with params='%s'\n",
        toolName.c_str(), params.c_str());

    std::string url = tool->url;

    if (tool->url.find("world://") == 0) {
        result.success = true;
        result.status_code = 200;
        result.body = "{\"tool\":\"" + toolName + "\",\"params\":\"" + params + "\"}";
        return result;
    }

    auto& client = ApiClient::instance();
    client.initialize();

    if (tool->method == "POST") {
        result = client.post(url, params, tool->content_type, tool->timeout_ms);
    } else {
        std::string fullUrl = url;
        if (!params.empty()) {
            fullUrl += (url.find('?') == std::string::npos ? "?" : "&") + params;
        }
        result = client.get(fullUrl, tool->timeout_ms);
    }

    result.tool_name = toolName;
    return result;
}

ApiCallResult ApiToolRegistry::executeWithRetry(const std::string& toolName,
                                                 const std::string& params,
                                                 int maxRetries) {
    ApiCallResult result;
    for (int i = 0; i <= maxRetries; i++) {
        result = executeTool(toolName, params);
        if (result.success) return result;
        if (i < maxRetries) {
            fprintf(stdout, "[ApiTool] Retry %d/%d for '%s'\n",
                i + 1, maxRetries, toolName.c_str());
            Sleep(500);
        }
    }
    return result;
}

std::string ApiToolRegistry::formatToolResult(const ApiCallResult& result) const {
    if (result.success) {
        return "[工具调用结果: " + result.tool_name + "]\n" + result.body;
    }
    return "[工具调用失败: " + result.tool_name + "] " + result.error;
}

size_t ApiToolRegistry::toolCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tools.size();
}

} // namespace npc
