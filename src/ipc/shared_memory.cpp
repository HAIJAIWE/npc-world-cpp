#include "shared_memory.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstring>
#include <utility>

namespace npc {

// ============================================================================
// Construction / Destruction
// ============================================================================

SharedMemory::SharedMemory() = default;

SharedMemory::~SharedMemory() {
    shutdown();
}

SharedMemory::SharedMemory(SharedMemory&& other) noexcept
    : m_mapped_view(other.m_mapped_view)
    , m_shm_base(other.m_shm_base)
    , m_total_size(other.m_total_size)
    , m_header(other.m_header)
    , m_ring_buffer(other.m_ring_buffer)
    , m_file_handle(other.m_file_handle)
    , m_mapping_handle(other.m_mapping_handle)
    , m_last_error(std::move(other.m_last_error))
{
    other.m_mapped_view    = nullptr;
    other.m_shm_base       = nullptr;
    other.m_header         = nullptr;
    other.m_ring_buffer    = nullptr;
    other.m_file_handle    = nullptr;
    other.m_mapping_handle = nullptr;
}

SharedMemory& SharedMemory::operator=(SharedMemory&& other) noexcept {
    if (this != &other) {
        shutdown();
        m_mapped_view    = other.m_mapped_view;
        m_shm_base       = other.m_shm_base;
        m_total_size     = other.m_total_size;
        m_header         = other.m_header;
        m_ring_buffer    = other.m_ring_buffer;
        m_file_handle    = other.m_file_handle;
        m_mapping_handle = other.m_mapping_handle;
        m_last_error     = std::move(other.m_last_error);

        other.m_mapped_view    = nullptr;
        other.m_shm_base       = nullptr;
        other.m_header         = nullptr;
        other.m_ring_buffer    = nullptr;
        other.m_file_handle    = nullptr;
        other.m_mapping_handle = nullptr;
    }
    return *this;
}

// ============================================================================
// Initialize / Shutdown
// ============================================================================

bool SharedMemory::initialize(SharedMemoryMode mode,
                               const wchar_t* name,
                               uint32_t total_size) {
    if (m_mapped_view) {
        m_last_error = "Already initialized";
        return false;
    }

    m_total_size = total_size;

    if (mode == SharedMemoryMode::Server) {
        // Create file mapping backed by system paging file
        HANDLE hMapping = CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(total_size),
            name
        );
        if (!hMapping) {
            m_last_error = "CreateFileMappingW failed (server). GLE="
                         + std::to_string(GetLastError());
            return false;
        }
        m_mapping_handle = hMapping;
    } else {
        // Open existing mapping
        HANDLE hMapping = OpenFileMappingW(
            FILE_MAP_ALL_ACCESS,
            FALSE,
            name
        );
        if (!hMapping) {
            m_last_error = "OpenFileMappingW failed (client). GLE="
                         + std::to_string(GetLastError());
            return false;
        }
        m_mapping_handle = hMapping;
    }

    // Map the entire region into our address space
    LPVOID view = MapViewOfFile(
        static_cast<HANDLE>(m_mapping_handle),
        FILE_MAP_ALL_ACCESS,
        0, 0,
        total_size
    );
    if (!view) {
        m_last_error = "MapViewOfFile failed. GLE="
                     + std::to_string(GetLastError());
        shutdown();
        return false;
    }

    m_mapped_view = view;
    m_shm_base    = static_cast<uint8_t*>(view);

    // Zero-initialize in server mode
    if (mode == SharedMemoryMode::Server) {
        std::memset(m_shm_base, 0, total_size);
    }

    // Set up typed pointers
    m_header      = reinterpret_cast<SharedMemoryHeader*>(m_shm_base);
    m_ring_buffer = m_shm_base + NPC_SHM_HEADER_SIZE;

    // Write metadata (server only)
    if (mode == SharedMemoryMode::Server) {
        m_header->buffer_size         = NPC_SHM_RING_SIZE;
        m_header->write_pos           = 0;
        m_header->read_pos            = 0;
        m_header->has_data.store(0, std::memory_order_relaxed);
        m_header->has_request.store(0, std::memory_order_relaxed);
        m_header->done.store(0, std::memory_order_relaxed);
        m_header->active_request_id   = 0;
        m_header->request_payload_size = 0;
        m_header->result_total_tokens  = 0;
        std::memset(m_header->error_msg, 0, sizeof(m_header->error_msg));
    }

    return true;
}

void SharedMemory::shutdown() {
    if (m_mapped_view) {
        UnmapViewOfFile(m_mapped_view);
        m_mapped_view = nullptr;
    }
    if (m_mapping_handle) {
        CloseHandle(static_cast<HANDLE>(m_mapping_handle));
        m_mapping_handle = nullptr;
    }
    m_shm_base    = nullptr;
    m_header      = nullptr;
    m_ring_buffer = nullptr;
    m_total_size  = 0;
}

// ============================================================================
// Ring buffer — write_entry
// ============================================================================

uint32_t SharedMemory::write_entry(uint32_t request_id,
                                    const void* payload,
                                    uint32_t payload_size) {
    if (!m_header || !m_ring_buffer) return 0;

    const uint32_t total_bytes = RING_ENTRY_HEADER_SIZE + payload_size;
    const uint32_t buf_size    = m_header->buffer_size;

    // Check free space
    uint32_t free_bytes = ring_free_space(
        m_header->write_pos,
        m_header->read_pos,
        buf_size);
    if (free_bytes < total_bytes) return 0;  // buffer full

    uint32_t wp = m_header->write_pos;

    // Build entry header in stack
    RingBufferEntry entry;
    entry.magic        = RING_MAGIC;
    entry.request_id   = request_id;
    entry.payload_size = payload_size;

    // Helper lambda: copy `size` bytes from `src` at ring offset `offset`
    auto ring_copy = [this, buf_size](uint32_t offset,
                                       const void* src,
                                       uint32_t size) {
        uint32_t first = (buf_size - offset >= size) ? size : (buf_size - offset);
        std::memcpy(m_ring_buffer + offset, src, first);
        if (first < size) {
            std::memcpy(m_ring_buffer,
                        static_cast<const uint8_t*>(src) + first,
                        size - first);
        }
    };

    // Write header
    ring_copy(wp, &entry, sizeof(RingBufferEntry));
    // Write payload
    uint32_t payload_offset = (wp + sizeof(RingBufferEntry)) % buf_size;
    ring_copy(payload_offset, payload, payload_size);

    // Advance write cursor
    m_header->write_pos = ring_advance(wp, total_bytes, buf_size);

    return total_bytes;
}

// ============================================================================
// Ring buffer — read_entry
// ============================================================================

uint32_t SharedMemory::read_entry(uint32_t& out_request_id,
                                   void* payload_buffer,
                                   uint32_t max_payload_size) {
    if (!m_header || !m_ring_buffer) return 0;

    const uint32_t rp       = m_header->read_pos;
    const uint32_t wp       = m_header->write_pos;
    const uint32_t buf_size = m_header->buffer_size;

    // Need at least a header
    uint32_t avail = ring_available(wp, rp, buf_size);
    if (avail < sizeof(RingBufferEntry)) return 0;

    // Helper lambda: read `size` bytes from ring offset `offset` into `dst`
    auto ring_read = [this, buf_size](uint32_t offset,
                                       void* dst,
                                       uint32_t size) {
        uint32_t first = (buf_size - offset >= size) ? size : (buf_size - offset);
        std::memcpy(dst, m_ring_buffer + offset, first);
        if (first < size) {
            std::memcpy(static_cast<uint8_t*>(dst) + first,
                        m_ring_buffer,
                        size - first);
        }
    };

    // Read header
    RingBufferEntry entry{};
    ring_read(rp, &entry, sizeof(RingBufferEntry));

    // Validate
    if (entry.magic != RING_MAGIC) {
        // Corrupted — resync to write_pos
        m_header->read_pos = wp;
        return 0;
    }

    const uint32_t total_bytes = sizeof(RingBufferEntry) + entry.payload_size;
    if (avail < total_bytes) return 0;  // partial write, wait

    // Read payload
    uint32_t copy_size = (entry.payload_size < max_payload_size)
                             ? entry.payload_size
                             : max_payload_size;
    uint32_t payload_offset = (rp + sizeof(RingBufferEntry)) % buf_size;
    ring_read(payload_offset, payload_buffer, copy_size);

    // Advance read cursor
    m_header->read_pos = ring_advance(rp, total_bytes, buf_size);

    out_request_id = entry.request_id;
    return copy_size;
}

// ============================================================================
// Ring buffer reset
// ============================================================================

void SharedMemory::reset_ring_buffer() {
    if (m_header) {
        m_header->write_pos = 0;
        m_header->read_pos  = 0;
        m_header->has_data.store(0, std::memory_order_release);
    }
}

// ============================================================================
// Error reporting
// ============================================================================

void SharedMemory::set_error(const char* msg) {
    if (m_header) {
        std::strncpy(m_header->error_msg, msg, sizeof(m_header->error_msg) - 1);
        m_header->error_msg[sizeof(m_header->error_msg) - 1] = '\0';
    }
}

} // namespace npc