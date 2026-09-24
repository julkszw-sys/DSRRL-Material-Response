#pragma once

#include "dsrrl/operators/material_response/material_response_island.hpp"

#include <cstdint>

namespace dsrrl::operators::env_spec {

enum class action : std::uint8_t {
    preserve_host = 0,
    suppress_dsr_only,
    activate_ptde_bridge
};

struct decision {
    action selected = action::preserve_host;
    bool ptde_bridge_required = false;
};

class env_spec_island {
public:
    static decision gate(
        material_response::ptde_envspec_presence presence,
        bool ptde_bridge_ready) noexcept;
};

} // namespace dsrrl::operators::env_spec
