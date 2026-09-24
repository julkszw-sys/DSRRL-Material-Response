#include "dsrrl/operators/env_spec/env_spec_island.hpp"

namespace dsrrl::operators::env_spec {

decision env_spec_island::gate(
    material_response::ptde_envspec_presence presence,
    bool ptde_bridge_ready) noexcept
{
    using material_response::ptde_envspec_presence;

    switch (presence) {
    case ptde_envspec_presence::absent:
        return {action::suppress_dsr_only, false};

    case ptde_envspec_presence::present:
        if (ptde_bridge_ready)
            return {action::activate_ptde_bridge, true};
        return {action::preserve_host, true};

    case ptde_envspec_presence::unknown:
    default:
        return {action::preserve_host, false};
    }
}

decision env_spec_island::gate(
    const material_response::mtd_semantic_query &query,
    bool ptde_bridge_ready) noexcept
{
    return gate(
        material_response::mtd_envspec_presence(query),
        ptde_bridge_ready);
}

} // namespace dsrrl::operators::env_spec
