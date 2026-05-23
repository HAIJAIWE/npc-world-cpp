#pragma once

#include "database.h"
#include "../types/npc.h"

#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace npc {

class NPCRepository {
public:
    explicit NPCRepository(std::shared_ptr<Database> db);
    ~NPCRepository() = default;

    bool insert(NPCEntity& entity);

    std::vector<NPCEntity> fetch_all();

    std::optional<NPCEntity> fetch_by_id(const std::string& id);

    std::vector<NPCEntity> search_by_name(const std::string& pattern);

    int32_t count();

    bool update(const NPCEntity& entity);

    bool remove(const std::string& id);

private:
    static NPCEntity row_to_entity(sqlite3_stmt* stmt);

    std::shared_ptr<Database> m_db;
};

} // namespace npc
