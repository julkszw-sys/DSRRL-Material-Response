#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace dsrrl::runtime::texture_identity_transport {

struct hook_status {
    bool provenance_ok = false;
    bool name_hook_armed = false;
    bool clear_hook_armed = false;
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

} // namespace dsrrl::runtime::texture_identity_transport
