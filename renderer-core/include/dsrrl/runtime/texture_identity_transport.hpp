#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace dsrrl::runtime::texture_identity_transport {

struct hook_status {
    bool provenance_ok = false;
    bool name_hook_armed = false;
    bool clear_hook_armed = false;
    bool packet_hook_armed = false;
    bool decoder_hook_armed = false;
    bool restore_failed = false;
};

bool install() noexcept;
void uninstall() noexcept;
hook_status status() noexcept;

// Snapshot the exact logical texture name currently in the certified loader
// scope. Returns false when no exact name is active.
bool snapshot(std::wstring &logical_name) noexcept;

// Zero-allocation snapshot for resource lifecycle routing. The returned view
// points at thread-local transport storage and is valid only until the next
// texture-name hook event on the same thread.
bool snapshot_raw(
    const wchar_t *&logical_name,
    std::size_t &length) noexcept;

// Opt-in SPC25 producer/transport census. No name guesses and no pointer
// lifetimes are exported; counts alone diagnose the semantic handoff cut.
struct texture_name_liveness {
    std::uint64_t hook_calls = 0u;
    std::uint64_t names_captured = 0u;
    std::uint64_t names_cleared = 0u;
    std::uint64_t name_snapshots = 0u;
    std::uint64_t packet_writer_calls = 0u;
    std::uint64_t packet_named_in_scope = 0u;
    std::uint64_t cache_name_equal = 0u;
    std::uint64_t cache_name_different = 0u;
    std::uint64_t cache_name_unreadable = 0u;
    std::uint64_t decode_calls = 0u;
    std::uint64_t decode_payload_readable = 0u;
    std::uint64_t decode_writer_pointer_seen = 0u;
};
texture_name_liveness liveness() noexcept;

} // namespace dsrrl::runtime::texture_identity_transport
