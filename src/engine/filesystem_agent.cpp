#include "engine/filesystem_agent.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <ctime>

namespace fs = std::filesystem;

namespace npc {

const std::vector<std::string> FileSystemAgent::BLOCKED_PATTERNS = {
    "C:\\Windows",
    "C:\\Windows\\System32",
    "C:\\Program Files",
    "C:\\Program Files (x86)",
    "C:\\ProgramData\\Microsoft",
    "/etc",
    "/bin",
    "/sbin",
    "/usr/bin",
    "/usr/sbin",
    "/boot",
    "/sys",
    "/proc",
    "AppData\\Roaming\\Microsoft",
    ".ssh",
    ".gnupg"
};

FileSystemAgent& FileSystemAgent::instance() {
    static FileSystemAgent inst;
    return inst;
}

void FileSystemAgent::configure(const std::string& workspace_root) {
    std::lock_guard<std::mutex> lock(m_mutex);
    fs::path p(workspace_root);
    if (!fs::exists(p)) {
        fs::create_directories(p);
    }
    m_workspace = fs::absolute(p).string();
}

bool FileSystemAgent::isInWorkspace(const std::string& path) const {
    std::string resolved = resolvePath(path);
    if (resolved.empty()) return false;
    return resolved.rfind(m_workspace, 0) == 0;
}

bool FileSystemAgent::isPathSafe(const std::string& path) const {
    if (m_workspace.empty()) return false;
    if (!isInWorkspace(path)) return false;

    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto& pattern : BLOCKED_PATTERNS) {
        std::string lower_pattern = pattern;
        std::transform(lower_pattern.begin(), lower_pattern.end(), lower_pattern.begin(), ::tolower);
        if (lower.find(lower_pattern) != std::string::npos) {
            return false;
        }
    }

    return true;
}

std::string FileSystemAgent::resolvePath(const std::string& path) const {
    if (m_workspace.empty()) return "";
    fs::path ws(m_workspace);
    fs::path target(path);

    if (target.is_absolute()) {
        return target.string();
    }

    fs::path combined = ws / target;
    std::error_code ec;
    fs::path canonical = fs::weakly_canonical(combined, ec);
    if (ec) return combined.string();

    return canonical.string();
}

std::string FileSystemAgent::relativePath(const std::string& path) const {
    if (m_workspace.empty()) return path;
    std::string rel = path;
    if (rel.rfind(m_workspace, 0) == 0) {
        rel = rel.substr(m_workspace.length());
        while (!rel.empty() && (rel[0] == '\\' || rel[0] == '/')) {
            rel = rel.substr(1);
        }
    }
    return rel;
}

void FileSystemAgent::setProgressCallback(FsProgressCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progress_cb = std::move(cb);
}

FsResult FileSystemAgent::execute(const FsOpRequest& req) {
    switch (req.type) {
        case FsOpType::LIST:   return listDir(req.path, req.recursive, req.max_depth);
        case FsOpType::READ:   return readFile(req.path);
        case FsOpType::WRITE:  return writeFile(req.path, req.content);
        case FsOpType::MOVE:   return moveFile(req.path, req.dest_path);
        case FsOpType::COPY:   return copyFile(req.path, req.dest_path);
        case FsOpType::DELETE: return deleteFile(req.path);
        case FsOpType::MKDIR:  return makeDir(req.path);
        case FsOpType::EXISTS: return fileExists(req.path);
        case FsOpType::SEARCH: return searchFiles(req.pattern, req.path);
        case FsOpType::STAT:   return fileStat(req.path);
    }
    FsResult r;
    r.success = false;
    r.error = "未知操作类型";
    return r;
}

FsResult FileSystemAgent::listDir(const std::string& path, bool recursive, int max_depth) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_ops++;

    FsResult result;
    std::string resolved = resolvePath(path);
    if (!isPathSafe(resolved)) {
        result.error = "路径不在工作区内或访问被拒绝: " + path;
        m_stats.errors++;
        return result;
    }

    fs::path dir(resolved);
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        result.error = "目录不存在: " + path;
        m_stats.errors++;
        return result;
    }

    try {
        if (recursive) {
            int depth = 0;
            for (auto it = fs::recursive_directory_iterator(dir);
                 it != fs::recursive_directory_iterator(); ++it) {
                if (it.depth() > max_depth) {
                    it.disable_recursion_pending();
                    continue;
                }
                FsResult::FileEntry entry;
                entry.name = it->path().filename().string();
                entry.path = relativePath(it->path().string());
                entry.is_dir = it->is_directory();
                entry.size = it->is_regular_file() ? it->file_size() : 0;
                auto ftime = fs::last_write_time(*it);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                entry.modified_at = std::chrono::duration_cast<std::chrono::seconds>(
                    sctp.time_since_epoch()).count();
                result.entries.push_back(entry);
            }
        } else {
            for (const auto& it : fs::directory_iterator(dir)) {
                FsResult::FileEntry entry;
                entry.name = it.path().filename().string();
                entry.path = relativePath(it.path().string());
                entry.is_dir = it.is_directory();
                entry.size = it.is_regular_file() ? it.file_size() : 0;
                auto ftime = fs::last_write_time(it);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                entry.modified_at = std::chrono::duration_cast<std::chrono::seconds>(
                    sctp.time_since_epoch()).count();
                result.entries.push_back(entry);
            }
        }
        result.success = true;
    } catch (const std::exception& e) {
        result.error = std::string("列举目录失败: ") + e.what();
        m_stats.errors++;
    }

    return result;
}

FsResult FileSystemAgent::readFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_ops++;
    m_stats.read_ops++;

    FsResult result;
    std::string resolved = resolvePath(path);
    if (!isPathSafe(resolved)) {
        result.error = "路径不在工作区内或访问被拒绝: " + path;
        m_stats.errors++;
        return result;
    }

    fs::path file(resolved);
    if (!fs::exists(file) || !fs::is_regular_file(file)) {
        result.error = "文件不存在: " + path;
        m_stats.errors++;
        return result;
    }

    try {
        std::ifstream in(file, std::ios::binary | std::ios::ate);
        if (!in) {
            result.error = "无法打开文件: " + path;
            m_stats.errors++;
            return result;
        }

        auto size = in.tellg();
        in.seekg(0, std::ios::beg);

        std::string content((size_t)size, '\0');
        in.read(&content[0], size);
        in.close();

        result.content = std::move(content);
        result.file_size = size;
        result.success = true;
        m_stats.bytes_read += size;
    } catch (const std::exception& e) {
        result.error = std::string("读取文件失败: ") + e.what();
        m_stats.errors++;
    }

    return result;
}

FsResult FileSystemAgent::writeFile(const std::string& path, const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_ops++;
    m_stats.write_ops++;

    FsResult result;
    std::string resolved = resolvePath(path);
    if (!isPathSafe(resolved)) {
        result.error = "路径不在工作区内或访问被拒绝: " + path;
        m_stats.errors++;
        return result;
    }

    try {
        fs::path file(resolved);
        auto parent = file.parent_path();
        if (!fs::exists(parent)) {
            fs::create_directories(parent);
        }

        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        if (!out) {
            result.error = "无法创建文件: " + path;
            m_stats.errors++;
            return result;
        }

        out.write(content.data(), content.size());
        out.close();

        result.success = true;
        result.file_size = content.size();
        m_stats.bytes_written += content.size();
    } catch (const std::exception& e) {
        result.error = std::string("写入文件失败: ") + e.what();
        m_stats.errors++;
    }

    return result;
}

FsResult FileSystemAgent::moveFile(const std::string& src, const std::string& dest) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_ops++;

    FsResult result;
    std::string resolved_src = resolvePath(src);
    std::string resolved_dest = resolvePath(dest);
    if (!isPathSafe(resolved_src) || !isPathSafe(resolved_dest)) {
        result.error = "路径不在工作区内或访问被拒绝";
        m_stats.errors++;
        return result;
    }

    try {
        fs::path dest_parent = fs::path(resolved_dest).parent_path();
        if (!fs::exists(dest_parent)) {
            fs::create_directories(dest_parent);
        }
        fs::rename(resolved_src, resolved_dest);
        result.success = true;
    } catch (const std::exception& e) {
        result.error = std::string("移动文件失败: ") + e.what();
        m_stats.errors++;
    }

    return result;
}

FsResult FileSystemAgent::copyFile(const std::string& src, const std::string& dest) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_ops++;

    FsResult result;
    std::string resolved_src = resolvePath(src);
    std::string resolved_dest = resolvePath(dest);
    if (!isPathSafe(resolved_src) || !isPathSafe(resolved_dest)) {
        result.error = "路径不在工作区内或访问被拒绝";
        m_stats.errors++;
        return result;
    }

    try {
        fs::path dest_parent = fs::path(resolved_dest).parent_path();
        if (!fs::exists(dest_parent)) {
            fs::create_directories(dest_parent);
        }
        fs::copy(resolved_src, resolved_dest,
                  fs::copy_options::overwrite_existing | fs::copy_options::recursive);
        result.success = true;
    } catch (const std::exception& e) {
        result.error = std::string("复制文件失败: ") + e.what();
        m_stats.errors++;
    }

    return result;
}

FsResult FileSystemAgent::deleteFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_ops++;

    FsResult result;
    std::string resolved = resolvePath(path);
    if (!isPathSafe(resolved)) {
        result.error = "路径不在工作区内或访问被拒绝: " + path;
        m_stats.errors++;
        return result;
    }

    try {
        fs::path file(resolved);
        if (fs::is_directory(file)) {
            fs::remove_all(file);
        } else {
            fs::remove(file);
        }
        result.success = true;
    } catch (const std::exception& e) {
        result.error = std::string("删除失败: ") + e.what();
        m_stats.errors++;
    }

    return result;
}

FsResult FileSystemAgent::makeDir(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_ops++;

    FsResult result;
    std::string resolved = resolvePath(path);
    if (!isPathSafe(resolved)) {
        result.error = "路径不在工作区内或访问被拒绝: " + path;
        m_stats.errors++;
        return result;
    }

    try {
        fs::create_directories(resolved);
        result.success = true;
    } catch (const std::exception& e) {
        result.error = std::string("创建目录失败: ") + e.what();
        m_stats.errors++;
    }

    return result;
}

FsResult FileSystemAgent::fileExists(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_ops++;

    FsResult result;
    std::string resolved = resolvePath(path);
    if (!isPathSafe(resolved)) {
        result.error = "路径不在工作区内或访问被拒绝: " + path;
        m_stats.errors++;
        return result;
    }

    fs::path file(resolved);
    result.success = true;
    result.content = fs::exists(file) ? "true" : "false";
    if (fs::exists(file)) {
        result.file_size = fs::is_regular_file(file) ? fs::file_size(file) : 0;
    }
    return result;
}

FsResult FileSystemAgent::searchFiles(const std::string& pattern, const std::string& root) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_ops++;

    FsResult result;
    std::string search_root = root.empty() ? m_workspace : resolvePath(root);
    if (!isPathSafe(search_root)) {
        result.error = "路径不在工作区内或访问被拒绝";
        m_stats.errors++;
        return result;
    }

    try {
        std::regex name_regex;
        try {
            std::string escaped = std::regex_replace(pattern, std::regex(R"([.^$|()\[\]{}*+?\\])"), R"(\$&)");
            size_t pos = 0;
            while ((pos = escaped.find("\\*", pos)) != std::string::npos) {
                escaped.replace(pos, 2, ".*");
                pos += 2;
            }
            pos = 0;
            while ((pos = escaped.find("\\?", pos)) != std::string::npos) {
                escaped.replace(pos, 2, ".");
                pos += 1;
            }
            name_regex = std::regex(escaped, std::regex::icase);
        } catch (...) {
            std::string lower_pattern = pattern;
            std::transform(lower_pattern.begin(), lower_pattern.end(), lower_pattern.begin(), ::tolower);
            for (const auto& entry : fs::recursive_directory_iterator(search_root)) {
                std::string name = entry.path().filename().string();
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                if (name.find(lower_pattern) != std::string::npos) {
                    FsResult::FileEntry fe;
                    fe.name = entry.path().filename().string();
                    fe.path = relativePath(entry.path().string());
                    fe.is_dir = entry.is_directory();
                    fe.size = entry.is_regular_file() ? entry.file_size() : 0;
                    result.entries.push_back(fe);
                }
            }
            result.success = true;
            return result;
        }

        for (const auto& entry : fs::recursive_directory_iterator(search_root)) {
            std::string name = entry.path().filename().string();
            if (std::regex_search(name, name_regex)) {
                FsResult::FileEntry fe;
                fe.name = name;
                fe.path = relativePath(entry.path().string());
                fe.is_dir = entry.is_directory();
                fe.size = entry.is_regular_file() ? entry.file_size() : 0;
                result.entries.push_back(fe);
            }
        }

        result.success = true;
    } catch (const std::exception& e) {
        result.error = std::string("搜索失败: ") + e.what();
        m_stats.errors++;
    }

    return result;
}

FsResult FileSystemAgent::fileStat(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.total_ops++;

    FsResult result;
    std::string resolved = resolvePath(path);
    if (!isPathSafe(resolved)) {
        result.error = "路径不在工作区内或访问被拒绝: " + path;
        m_stats.errors++;
        return result;
    }

    try {
        fs::path file(resolved);
        if (!fs::exists(file)) {
            result.error = "文件不存在";
            m_stats.errors++;
            return result;
        }

        result.success = true;
        result.file_size = fs::is_regular_file(file) ? fs::file_size(file) : 0;
        auto ftime = fs::last_write_time(file);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        result.modified_at = std::chrono::duration_cast<std::chrono::seconds>(
            sctp.time_since_epoch()).count();

        std::ostringstream oss;
        oss << "路径: " << relativePath(resolved) << "\n";
        oss << "大小: " << result.file_size << " bytes\n";
        oss << "类型: " << (fs::is_directory(file) ? "目录" : "文件") << "\n";
        oss << "修改时间: " << result.modified_at;
        result.content = oss.str();
    } catch (const std::exception& e) {
        result.error = std::string("获取文件信息失败: ") + e.what();
        m_stats.errors++;
    }

    return result;
}

std::string FsResult::summary() const {
    if (!success) return "失败: " + error;
    std::ostringstream oss;
    if (!content.empty()) {
        oss << content;
    } else if (!entries.empty()) {
        oss << entries.size() << " 个项目:\n";
        for (const auto& e : entries) {
            oss << "  " << (e.is_dir ? "[DIR]" : "[FILE]") << " " << e.name
                << " (" << e.size << " bytes)\n";
        }
    }
    return oss.str();
}

} // namespace npc
