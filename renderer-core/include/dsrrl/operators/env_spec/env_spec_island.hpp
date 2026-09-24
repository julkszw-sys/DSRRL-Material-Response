#pragma once

#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/known_operator_island.hpp"
#include "dsrrl/operators/env_spec/no_spc_envspec_delete.hpp"
#include "dsrrl/operators/env_spec/legacy_runtime_readiness.hpp"

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

inline constexpr known_operator_island legacy{core::operator_id::env_spec};
inline constexpr known_operator_island no_spc_delete{core::operator_id::envspec_nospc_delete};
inline constexpr known_operator_island pmetal_diagnostic{core::operator_id::envspec_pmetal_diagnostic};

} // namespace dsrrl::operators::env_spec
