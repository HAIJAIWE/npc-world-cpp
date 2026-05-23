#include "engine/user_preferences.h"
#include "db/database.h"
#include <sqlite3.h>
#include <sstream>
#include <regex>
#include <ctime>

UserPreferences& UserPreferences::instance() {
    static UserPreferences inst;
    return inst;
}

bool UserPreferences::initialize(std::shared_ptr<npc::Database> db) {
    m_db = db;
    if (!m_db || !m_db->is_open()) return false;
    ensureTable();
    loadFromDb();
    m_initialized = true;
    return true;
}

void UserPreferences::ensureTable() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS user_preferences (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL,
            confidence REAL DEFAULT 0.5,
            updated_at INTEGER NOT NULL
        )
    )";
    sqlite3* handle = m_db->handle();
    if (handle) {
        char* err = nullptr;
        sqlite3_exec(handle, sql, nullptr, nullptr, &err);
        if (err) sqlite3_free(err);
    }
}

void UserPreferences::set(const std::string& key, const std::string& value, float confidence) {
    if (!m_initialized) return;

    PreferenceItem item;
    item.key = key;
    item.value = value;
    item.confidence = confidence;
    item.updated_at = (int64_t)time(nullptr);

    saveToDb(key, value, confidence);
}

std::string UserPreferences::get(const std::string& key, const std::string& default_val) const {
    if (!m_initialized) return default_val;

    sqlite3* handle = m_db->handle();
    if (!handle) return default_val;

    std::string sql = "SELECT value FROM user_preferences WHERE key = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(handle, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return default_val;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    std::string result = default_val;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* val = (const char*)sqlite3_column_text(stmt, 0);
        if (val) result = val;
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<UserPreferences::PreferenceItem> UserPreferences::all() const {
    std::vector<PreferenceItem> results;
    if (!m_initialized) return results;

    sqlite3* handle = m_db->handle();
    if (!handle) return results;

    const char* sql = "SELECT key, value, confidence, updated_at FROM user_preferences ORDER BY updated_at DESC";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) return results;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PreferenceItem item;
        item.key = (const char*)sqlite3_column_text(stmt, 0);
        item.value = (const char*)sqlite3_column_text(stmt, 1);
        item.confidence = (float)sqlite3_column_double(stmt, 2);
        item.updated_at = sqlite3_column_int64(stmt, 3);
        results.push_back(item);
    }
    sqlite3_finalize(stmt);
    return results;
}

void UserPreferences::extractFromMessage(const std::string& user_message) {
    if (!m_initialized || user_message.empty()) return;

    struct Rule { std::string pattern; std::string key; std::string value; float conf; };
    static const std::vector<Rule> rules = {
        {"简洁",          "style",          "concise",   0.6f},
        {"详细",          "style",          "detailed",  0.6f},
        {"啰嗦",          "style",          "verbose",   0.6f},
        {"现代",          "code_style",     "modern",    0.6f},
        {"经典",          "code_style",     "classic",   0.6f},
        {"中文",          "language",       "zh-CN",     0.7f},
        {"英文",          "language",       "en",        0.7f},
        {"友好",          "tone",           "friendly",  0.5f},
        {"正式",          "tone",           "formal",    0.5f},
        {"幽默",          "tone",           "humorous",  0.5f},
        {"玩家",          "audience",       "player",    0.5f},
        {"开发者",        "audience",       "developer", 0.5f},
        {"NPC",           "audience",       "npc",       0.5f},
        {"用 C++",        "prefer_lang",    "cpp",       0.7f},
        {"用 Python",     "prefer_lang",    "python",    0.7f},
        {"不改注释",      "add_comments",   "false",     0.7f},
        {"加注释",        "add_comments",   "true",      0.7f},
    };

    for (const auto& rule : rules) {
        if (user_message.find(rule.pattern) != std::string::npos) {
            set(rule.key, rule.value, rule.conf);
        }
    }
}

std::string UserPreferences::formatForPrompt() const {
    auto prefs = all();
    if (prefs.empty()) return "";

    std::ostringstream oss;
    oss << "[用户偏好]\n";
    for (const auto& p : prefs) {
        if (p.confidence >= 0.5f) {
            oss << "  " << p.key << " = " << p.value;
            if (p.confidence < 0.8f) oss << " (置信度: " << (int)(p.confidence * 100) << "%)";
            oss << "\n";
        }
    }
    return oss.str();
}

UserPreferences::PrefStats UserPreferences::stats() const {
    PrefStats s;
    s.total_prefs = 0;
    s.high_confidence = 0;
    for (const auto& p : all()) {
        s.total_prefs++;
        if (p.confidence >= 0.8f) s.high_confidence++;
    }
    return s;
}

void UserPreferences::loadFromDb() {
}

void UserPreferences::saveToDb(const std::string& key, const std::string& value, float confidence) {
    sqlite3* handle = m_db->handle();
    if (!handle) return;

    const char* sql = R"(
        INSERT OR REPLACE INTO user_preferences (key, value, confidence, updated_at)
        VALUES (?, ?, ?, ?)
    )";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(handle, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, confidence);
    sqlite3_bind_int64(stmt, 4, (int64_t)time(nullptr));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}