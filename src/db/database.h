#pragma once

#include <string>
#include <vector>
#include <functional>

struct sqlite3;
struct sqlite3_stmt;

namespace npc {

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) noexcept;
    Database& operator=(Database&&) noexcept;

    bool open(const std::string& path);

    void close();

    bool is_open() const { return m_db != nullptr; }

    sqlite3* handle() const { return m_db; }

    bool exec(const std::string& sql);

    sqlite3_stmt* prepare(const std::string& sql);

    bool execute_stmt(const std::string& sql);

    int64_t last_insert_rowid() const;

    int32_t changes() const;

    std::string last_error() const;

    bool begin_transaction();
    bool commit();
    bool rollback();

private:
    bool run_migrations();
    bool ensure_schema_version_table();
    int32_t current_schema_version();
    bool apply_migration(int32_t version, const std::string& sql);

    sqlite3* m_db = nullptr;
    std::string m_path;
};

} // namespace npc
