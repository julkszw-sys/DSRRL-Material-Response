#pragma once

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

} // namespace dsrrl::runtime::texture_identity_transport
