#pragma once
#include <cstdint>

namespace dsrrl::runtime::flver_identity_transport {

struct hook_status {
    bool provenance_ok = false;
    bool parser_armed = false;
    bool selector_armed = false;
    bool destructor_armed = false;
    bool restore_failed = false;
    bool selector_owner_enrichment = false;
};

bool install() noexcept;
void uninstall() noexcept;
hook_status status() noexcept;

} // namespace dsrrl::runtime::flver_identity_transport
