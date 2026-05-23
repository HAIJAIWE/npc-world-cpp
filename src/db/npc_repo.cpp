#include "npc_repo.h"

#include <sqlite3.h>

#include <cstdio>
#include <chrono>
#include <random>

namespace npc {

// ============================================================================
// Utility: generate a short random ID
// ============================================================================

static std::string generate_id() {
    static thread_local std::mt19937_64 rng(
        static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
        )
    );
    static const char hex[] = "0123456789abcdef";
    std::string id(16, '\0');
    for (int i = 0; i < 16; ++i) {
        id[i] = hex[rng() & 0xF];
    }
    return id;
}

static int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// ============================================================================
// Construction
// ============================================================================

NPCRepository::NPCRepository(std::shared_ptr<Database> db)
    : m_db(std::move(db)) {}

// ============================================================================
// Create
// ============================================================================

bool NPCRepository::insert(NPCEntity& entity) {
    if (!m_db || !m_db->is_open()) return false;

    if (entity.id.empty()) {
        entity.id = generate_id();
    }
    int64_t ts = now_ms();
    entity.created_at = ts;
    entity.updated_at = ts;

    const char* sql =
        "INSERT INTO npcs (id, name, aliases, age, gender, role, "
        "personality, background, knowledge, behavior, created_at, updated_at) "
        "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12)";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db->handle(), sql, -1, &stmt, nullptr);
    if (!stmt) return false;

    sqlite3_bind_text(stmt,  1,  entity.id.c_str(),          -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  2,  entity.name.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  3,  entity.aliases.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,   4,  entity.age);
    sqlite3_bind_text(stmt,  5,  entity.gender.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  6,  entity.role.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  7,  entity.personality.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  8,  entity.background.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  9,  entity.knowledge.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10,  entity.behavior.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 11, entity.created_at);
    sqlite3_bind_int64(stmt, 12, entity.updated_at);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[NPCRepo] Insert failed: %s\n", sqlite3_errmsg(m_db->handle()));
        return false;
    }
    return true;
}

// ============================================================================
// Read
// ============================================================================

NPCEntity NPCRepository::row_to_entity(sqlite3_stmt* stmt) {
    NPCEntity e;
    e.id          = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    e.name        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    const char* a = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    if (a) e.aliases = a;
    e.age         = sqlite3_column_int(stmt, 3);
    const char* g = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    if (g) e.gender = g;
    const char* r = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    if (r) e.role = r;
    const char* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    if (p) e.personality = p;
    const char* bg = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
    if (bg) e.background = bg;
    const char* k = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
    if (k) e.knowledge = k;
    const char* b = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
    if (b) e.behavior = b;
    e.created_at  = sqlite3_column_int64(stmt, 10);
    e.updated_at  = sqlite3_column_int64(stmt, 11);
    return e;
}

std::vector<NPCEntity> NPCRepository::fetch_all() {
    std::vector<NPCEntity> result;
    if (!m_db || !m_db->is_open()) return result;

    const char* sql =
        "SELECT id, name, aliases, age, gender, role, "
        "personality, background, knowledge, behavior, created_at, updated_at "
        "FROM npcs ORDER BY name ASC";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db->handle(), sql, -1, &stmt, nullptr);
    if (!stmt) return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(row_to_entity(stmt));
    }
    sqlite3_finalize(stmt);
    return result;
}

std::optional<NPCEntity> NPCRepository::fetch_by_id(const std::string& id) {
    if (!m_db || !m_db->is_open()) return std::nullopt;

    const char* sql =
        "SELECT id, name, aliases, age, gender, role, "
        "personality, background, knowledge, behavior, created_at, updated_at "
        "FROM npcs WHERE id = ?1";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db->handle(), sql, -1, &stmt, nullptr);
    if (!stmt) return std::nullopt;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<NPCEntity> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_entity(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<NPCEntity> NPCRepository::search_by_name(const std::string& pattern) {
    std::vector<NPCEntity> result;
    if (!m_db || !m_db->is_open()) return result;

    const char* sql =
        "SELECT id, name, aliases, age, gender, role, "
        "personality, background, knowledge, behavior, created_at, updated_at "
        "FROM npcs WHERE name LIKE ?1 ORDER BY name ASC";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db->handle(), sql, -1, &stmt, nullptr);
    if (!stmt) return result;

    std::string like = "%" + pattern + "%";
    sqlite3_bind_text(stmt, 1, like.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(row_to_entity(stmt));
    }
    sqlite3_finalize(stmt);
    return result;
}

int32_t NPCRepository::count() {
    if (!m_db || !m_db->is_open()) return 0;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db->handle(), "SELECT COUNT(*) FROM npcs", -1, &stmt, nullptr);
    if (!stmt) return 0;

    int32_t c = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        c = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return c;
}

// ============================================================================
// Update
// ============================================================================

bool NPCRepository::update(const NPCEntity& entity) {
    if (!m_db || !m_db->is_open()) return false;
    if (entity.id.empty()) return false;

    const char* sql =
        "UPDATE npcs SET name=?2, aliases=?3, age=?4, gender=?5, role=?6, "
        "personality=?7, background=?8, knowledge=?9, behavior=?10, "
        "updated_at=?11 WHERE id=?1";

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db->handle(), sql, -1, &stmt, nullptr);
    if (!stmt) return false;

    sqlite3_bind_text(stmt,  1,  entity.id.c_str(),          -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  2,  entity.name.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  3,  entity.aliases.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,   4,  entity.age);
    sqlite3_bind_text(stmt,  5,  entity.gender.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  6,  entity.role.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  7,  entity.personality.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  8,  entity.background.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  9,  entity.knowledge.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10,  entity.behavior.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 11, now_ms());

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[NPCRepo] Update failed: %s\n", sqlite3_errmsg(m_db->handle()));
        return false;
    }
    return m_db->changes() > 0;
}

// ============================================================================
// Delete
// ============================================================================

bool NPCRepository::remove(const std::string& id) {
    if (!m_db || !m_db->is_open()) return false;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_db->handle(), "DELETE FROM npcs WHERE id = ?1", -1, &stmt, nullptr);
    if (!stmt) return false;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE && m_db->changes() > 0;
}

} // namespace npc