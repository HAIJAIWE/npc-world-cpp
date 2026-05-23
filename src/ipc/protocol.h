#pragma once

#include <atomic>
#include <cstdint>

#define NPC_SHM_HEADER_SIZE      512
#define NPC_SHM_BUFFER_SIZE      (16 * 1024 * 1024)
#define NPC_SHM_RING_SIZE        (NPC_SHM_BUFFER_SIZE - NPC_SHM_HEADER_SIZE)

#define NPC_SHM_NAME             L"NPCWorld_Shm_TokenStream"

constexpr uint32_t RING_MAGIC     = 0xAA5500FF;

#pragma pack(push, 1)
struct RingBufferEntry {
    uint32_t magic;
    uint32_t request_id;
    uint32_t payload_size;
};
#pragma pack(pop)

static_assert(sizeof(RingBufferEntry) == 12, "RingBufferEntry must be 12 bytes");

#pragma pack(push, 1)
struct SharedMemoryHeader {
    uint32_t write_pos;
    uint32_t read_pos;
    uint32_t buffer_size;

    std::atomic<uint32_t> has_data;
    std::atomic<uint32_t> has_request;
    std::atomic<uint32_t> done;

    uint32_t active_request_id;
    uint32_t request_payload_size;
    uint32_t result_total_tokens;

    char     error_msg[256];

    uint8_t  _pad[220];
};
#pragma pack(pop)

static_assert(sizeof(SharedMemoryHeader) <= NPC_SHM_HEADER_SIZE,
              "SharedMemoryHeader must fit in NPC_SHM_HEADER_SIZE bytes");

constexpr uint32_t RING_ENTRY_HEADER_SIZE = sizeof(RingBufferEntry);

#include <cstddef>

namespace npc {

inline uint32_t ring_advance(uint32_t cursor, uint32_t bytes, uint32_t buffer_size) {
    return (cursor + bytes) % buffer_size;
}

inline uint32_t ring_available(uint32_t write_pos, uint32_t read_pos, uint32_t buffer_size) {
    if (write_pos >= read_pos)
        return write_pos - read_pos;
    else
        return buffer_size - read_pos + write_pos;
}

inline uint32_t ring_free_space(uint32_t write_pos, uint32_t read_pos, uint32_t buffer_size) {
    uint32_t used = ring_available(write_pos, read_pos, buffer_size);
    if (used == 0 && write_pos == read_pos) return buffer_size - 1;
    return buffer_size - used - 1;
}

} // namespace npc
