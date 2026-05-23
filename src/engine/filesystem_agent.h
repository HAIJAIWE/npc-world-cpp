#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <mutex>

namespace npc {

enum class FsOpType : uint8_t {
    LIST,
    READ,
    WRITE,
    MOVE,
    COPY,
    DELETE,
    MKDIR,
    EXISTS,
    SEARCH,
    STAT
};

struct FsResult {
    bool success = false;
    std::string content;
    std::string error;
    int64_t file_size = 0;
    int64_t modified_at = 0;

    struct FileEntry {
        std::string name;
        std::string path;
        bool is_dir = false;
        int64_t size = 0;
        int64_t modified_at = 0;
    };
    std::vector<FileEntry> entries;

    std::string summary() const;
};

struct FsOpRequest {
    FsOpType type;
    std::string path;
    std::string content;
    std::string dest_path;
    std::string pattern;
    bool recursive = false;
    int max_depth = 3;
};

using FsProgressCallback = std::function<void(const std::string& path, int progress, int total)>;

class FileSystemAgent {
public:
    static FileSystemAgent& instance();

    void configure(const std::string& workspace_root);
    const std::string& workspaceRoot() const { return m_workspace; }

    FsResult execute(const FsOpRequest& req);
    FsResult listDir(const std::string& path, bool recursive = false, int max_depth = 3);
    FsResult readFile(const std::string& path);
    FsResult writeFile(const std::string& path, const std::string& content);
    FsResult moveFile(const std::string& src, const std::string& dest);
    FsResult copyFile(const std::string& src, const std::string& dest);
    FsResult deleteFile(const std::string& path);
    FsResult makeDir(const std::string& path);
    FsResult fileExists(const std::string& path);
    FsResult searchFiles(const std::string& pattern, const std::string& root = "");
    FsResult fileStat(const std::string& path);

    bool isInWorkspace(const std::string& path) const;

    void setProgressCallback(FsProgressCallback cb);

    struct FsStats {
        int64_t total_ops = 0;
        int64_t read_ops = 0;
        int64_t write_ops = 0;
        int64_t errors = 0;
        int64_t bytes_read = 0;
        int64_t bytes_written = 0;
    };
    FsStats stats() const { return m_stats; }

private:
    FileSystemAgent() = default;
    FileSystemAgent(const FileSystemAgent&) = delete;
    FileSystemAgent& operator=(const FileSystemAgent&) = delete;

    std::string resolvePath(const std::string& path) const;
    std::string relativePath(const std::string& path) const;
    bool isPathSafe(const std::string& path) const;

    std::string m_workspace;
    FsProgressCallback m_progress_cb;
    FsStats m_stats;
    mutable std::mutex m_mutex;

    static const std::vector<std::string> BLOCKED_PATTERNS;
};

} // namespace npc
