#pragma once

#include "protocol.h"

#include <string>
#include <cstdint>

namespace npc {

enum class SharedMemoryMode {
    Server,
    Client
};

class SharedMemory {
public:
    SharedMemory();
    ~SharedMemory();

    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;
    SharedMemory(SharedMemory&& other) noexcept;
    SharedMemory& operator=(SharedMemory&& other) noexcept;

    bool initialize(SharedMemoryMode mode,
                    const wchar_t* name = NPC_SHM_NAME,
                    uint32_t total_size = NPC_SHM_BUFFER_SIZE);

    void shutdown();

    bool is_valid() const { return m_mapped_view != nullptr; }

    SharedMemoryHeader* header() { return m_header; }
    const SharedMemoryHeader* header() const { return m_header; }

    uint8_t* ring_buffer() { return m_ring_buffer; }
    const uint8_t* ring_buffer() const { return m_ring_buffer; }
    uint32_t ring_buffer_size() const { return NPC_SHM_RING_SIZE; }

    uint32_t write_entry(uint32_t request_id,
                         const void* payload,
                         uint32_t payload_size);

    uint32_t read_entry(uint32_t& out_request_id,
                        void* payload_buffer,
                        uint32_t max_payload_size);

    void signal_has_data()  { m_header->has_data.store(1, std::memory_order_release); }
    void clear_has_data()   { m_header->has_data.store(0, std::memory_order_release); }

    bool has_data() const { return m_header->has_data.load(std::memory_order_acquire) != 0; }

    void signal_done()      { m_header->done.store(1, std::memory_order_release); }
    void clear_done()       { m_header->done.store(0, std::memory_order_release); }
    bool is_done() const    { return m_header->done.load(std::memory_order_acquire) != 0; }

    void signal_has_request() { m_header->has_request.store(1, std::memory_order_release); }
    void clear_has_request()  { m_header->has_request.store(0, std::memory_order_release); }
    bool has_request() const  { return m_header->has_request.load(std::memory_order_acquire) != 0; }

    void reset_ring_buffer();

    void set_error(const char* msg);
    const char* get_error() const { return m_header->error_msg; }

private:
    void*       m_mapped_view   = nullptr;
    uint8_t*    m_shm_base      = nullptr;
    uint32_t    m_total_size    = 0;

    SharedMemoryHeader* m_header      = nullptr;
    uint8_t*            m_ring_buffer = nullptr;

    void*       m_file_handle    = nullptr;
    void*       m_mapping_handle = nullptr;

    std::string m_last_error;
};

} // namespace npc
