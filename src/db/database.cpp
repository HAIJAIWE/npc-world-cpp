#include "database.h"

#include <sqlite3.h>

#include <cstdio>
#include <utility>

namespace npc {

// ============================================================================
// Schema — all 6 tables from the design spec
// ============================================================================

// clang-format off
static const char* MIGRATION_V1 = R"SQL(
-- =====================================================
-- NPC World — Schema v1
-- Matches docs/superpowers/specs/2025-05-19-cpp-rewrite-design.md Section 4.1
-- =====================================================

-- NPC basic profile
CREATE TABLE IF NOT EXISTS npcs (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    aliases     TEXT,             -- JSON array
    age         INTEGER,
    gender      TEXT,
    role        TEXT,
    personality TEXT,             -- JSON object {traits, speaking_style, ...}
    background  TEXT,             -- JSON object {history, secrets, ...}
    knowledge   TEXT,             -- JSON object {domains, ignorance}
    behavior    TEXT,             -- JSON object {daily_routine, likes, ...}
    created_at  INTEGER NOT NULL DEFAULT (strftime('%s','now') * 1000),
    updated_at  INTEGER NOT NULL DEFAULT (strftime('%s','now') * 1000)
);

-- NPC memories (per-player)
CREATE TABLE IF NOT EXISTS memories (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    npc_id      TEXT NOT NULL,
    player_id   TEXT NOT NULL,
    memory_type TEXT DEFAULT 'general',
    content     TEXT NOT NULL,
    importance  REAL DEFAULT 0.5,
    timestamp   INTEGER NOT NULL,
    FOREIGN KEY (npc_id) REFERENCES npcs(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_memories_npc
    ON memories(npc_id, player_id, timestamp DESC);

-- World settings
CREATE TABLE IF NOT EXISTS worlds (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    era         TEXT,
    description TEXT,
    factions    TEXT,             -- JSON array
    locations   TEXT,             -- JSON array
    created_at  INTEGER NOT NULL DEFAULT (strftime('%s','now') * 1000),
    updated_at  INTEGER NOT NULL DEFAULT (strftime('%s','now') * 1000)
);

-- Plot chapters
CREATE TABLE IF NOT EXISTS plot_chapters (
    id            TEXT PRIMARY KEY,
    world_id      TEXT,
    title         TEXT NOT NULL,
    chapter_order INTEGER,
    conditions    TEXT,           -- JSON
    scenes        TEXT,           -- JSON array
    FOREIGN KEY (world_id) REFERENCES worlds(id) ON DELETE SET NULL
);

-- Conversation history
CREATE TABLE IF NOT EXISTS conversations (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    npc_id      TEXT NOT NULL,
    player_name TEXT NOT NULL,
    speaker     TEXT NOT NULL,    -- "user" | "npc" | "system"
    content     TEXT NOT NULL,
    timestamp   INTEGER NOT NULL,
    FOREIGN KEY (npc_id) REFERENCES npcs(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_conversations_npc
    ON conversations(npc_id, timestamp);

-- App settings (key-value)
CREATE TABLE IF NOT EXISTS settings (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
)SQL";
// clang-format on

// clang-format off
static const char* MIGRATION_V2 = R"SQL(
-- =====================================================
-- NPC World — Schema v2: NPC brain tables
-- Adds episodic memory, beliefs, procedural knowledge
 -- =====================================================

 -- NPC brain state (JSON blob for performance)
 CREATE TABLE IF NOT EXISTS npc_brains (
     npc_id      TEXT PRIMARY KEY,
     brain_json  TEXT NOT NULL,
     updated_at  INTEGER NOT NULL,
     FOREIGN KEY (npc_id) REFERENCES npcs(id) ON DELETE CASCADE
 );

 -- Episodic memories (rich event records)
CREATE TABLE IF NOT EXISTS episodic_memories (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    npc_id      TEXT NOT NULL,
    title       TEXT,
    description TEXT,
    emotional_impact REAL DEFAULT 0.0,
    participants TEXT,            -- JSON array of NPC/player IDs
    location    TEXT,
    timestamp   INTEGER NOT NULL,
    FOREIGN KEY (npc_id) REFERENCES npcs(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_episodic_npc ON episodic_memories(npc_id, timestamp DESC);

-- Beliefs (what the NPC thinks is true)
CREATE TABLE IF NOT EXISTS beliefs (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    npc_id      TEXT NOT NULL,
    belief_text TEXT NOT NULL,
    confidence  REAL DEFAULT 1.0,
    source      TEXT,             -- "observation" | "inference" | "received"
    created_at  INTEGER NOT NULL DEFAULT (strftime('%s','now') * 1000),
    updated_at  INTEGER NOT NULL DEFAULT (strftime('%s','now') * 1000),
    FOREIGN KEY (npc_id) REFERENCES npcs(id) ON DELETE CASCADE
);

-- Procedural knowledge (skills & routines)
CREATE TABLE IF NOT EXISTS procedural_knowledge (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    npc_id      TEXT NOT NULL,
    skill_name  TEXT NOT NULL,
    skill_type  TEXT DEFAULT 'general',
    proficiency REAL DEFAULT 0.0,
    steps       TEXT,             -- JSON array of step objects
    notes       TEXT,
    FOREIGN KEY (npc_id) REFERENCES npcs(id) ON DELETE CASCADE
);
)SQL";
// clang-format on

// ============================================================================
// Construction / Destruction
// ============================================================================

Database::~Database() {
    close();
}

Database::Database(Database&& other) noexcept
    : m_db(other.m_db)
    , m_path(std::move(other.m_path)) {
    other.m_db = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        close();
        m_db   = other.m_db;
        m_path = std::move(other.m_path);
        other.m_db = nullptr;
    }
    return *this;
}

// ============================================================================
// Open / Close
// ============================================================================

bool Database::open(const std::string& path) {
    if (m_db) {
        return false;  // already open
    }

    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        const char* err = m_db ? sqlite3_errmsg(m_db) : "unknown";
        fprintf(stderr, "[Database] Failed to open %s: %s\n", path.c_str(), err);
        if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
        return false;
    }

    m_path = path;

    // Enable WAL mode for better concurrent read performance
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA foreign_keys=ON");
    exec("PRAGMA busy_timeout=5000");

    // Run migrations
    if (!run_migrations()) {
        fprintf(stderr, "[Database] Migration failed. DB may be in inconsistent state.\n");
        close();
        return false;
    }

    return true;
}

void Database::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
    m_path.clear();
}

// ============================================================================
// Raw SQL execution
// ============================================================================

bool Database::exec(const std::string& sql) {
    if (!m_db) return false;

    char* err_msg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[Database] SQL error: %s\n", err_msg ? err_msg : "unknown");
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

sqlite3_stmt* Database::prepare(const std::string& sql) {
    if (!m_db) return nullptr;

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[Database] Prepare error: %s\n", sqlite3_errmsg(m_db));
        return nullptr;
    }
    return stmt;
}

bool Database::execute_stmt(const std::string& sql) {
    sqlite3_stmt* stmt = prepare(sql);
    if (!stmt) return false;

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE || rc == SQLITE_ROW;
}

int64_t Database::last_insert_rowid() const {
    return m_db ? sqlite3_last_insert_rowid(m_db) : 0;
}

int32_t Database::changes() const {
    return m_db ? sqlite3_changes(m_db) : 0;
}

std::string Database::last_error() const {
    return m_db ? std::string(sqlite3_errmsg(m_db)) : "no connection";
}

// ============================================================================
// Transactions
// ============================================================================

bool Database::begin_transaction() {
    return exec("BEGIN TRANSACTION");
}

bool Database::commit() {
    return exec("COMMIT");
}

bool Database::rollback() {
    return exec("ROLLBACK");
}

// ============================================================================
// Migration system
// ============================================================================

bool Database::ensure_schema_version_table() {
    return exec(
        "CREATE TABLE IF NOT EXISTS _schema_version ("
        "  version INTEGER PRIMARY KEY,"
        "  applied_at INTEGER NOT NULL DEFAULT (strftime('%s','now') * 1000)"
        ")"
    );
}

int32_t Database::current_schema_version() {
    sqlite3_stmt* stmt = prepare("SELECT MAX(version) FROM _schema_version");
    if (!stmt) return 0;

    int32_t ver = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ver = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return ver;
}

bool Database::apply_migration(int32_t version, const std::string& sql) {
    if (!exec(sql)) return false;

    sqlite3_stmt* stmt = prepare(
        "INSERT INTO _schema_version (version) VALUES (?)"
    );
    if (!stmt) return false;

    sqlite3_bind_int(stmt, 1, version);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool Database::run_migrations() {
    if (!ensure_schema_version_table()) return false;

    int32_t current = current_schema_version();

    // Migration 1: initial schema
    if (current < 1) {
        fprintf(stdout, "[Database] Applying migration v1 (initial schema)\n");
        if (!apply_migration(1, MIGRATION_V1)) {
            fprintf(stderr, "[Database] Migration v1 FAILED\n");
            return false;
        }
        fprintf(stdout, "[Database] Migration v1 applied successfully.\n");
    }

    // Migration 2: NPC brain tables
    if (current < 2) {
        fprintf(stdout, "[Database] Applying migration v2 (NPC brain tables)\n");
        if (!apply_migration(2, MIGRATION_V2)) {
            fprintf(stderr, "[Database] Migration v2 FAILED\n");
            return false;
        }
        fprintf(stdout, "[Database] Migration v2 applied successfully.\n");
    }

    return true;
}

} // namespace npc