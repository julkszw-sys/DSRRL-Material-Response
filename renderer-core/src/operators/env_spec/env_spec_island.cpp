#include "dsrrl/operators/env_spec/env_spec_island.hpp"

namespace dsrrl::operators::env_spec {

decision env_spec_island::gate(
    material_response::ptde_envspec_presence presence,
    bool ptde_bridge_ready) noexcept
{
    using material_response::ptde_envspec_presence;

    switch (presence) {
    case ptde_envspec_presence::absent:
        // Presence-only absence has no receiver/material identity proof and
        // therefore cannot authorize deletion of a DSR EnvSpec contribution.
        // Certified suppression is issued exclusively by the exact MTD gate
        // below (explicit_none + suppress_dsr_only_safe).
        return {action::preserve_host, false};

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
    const auto exact=
        material_response::classify_mtd_envspec_semantics(query);

    if(exact.exact_identity_match){
        switch(exact.router_state){
        case material_response::mtd_envspec_router_state::present:
            return gate(
                material_response::ptde_envspec_presence::present,
                ptde_bridge_ready);

        case material_response::mtd_envspec_router_state::explicit_none:
            // Semantic PTDE absence is not by itself carrier authorization.
            // Only the historical 20-record clean SPC_INSERT_ONLY cohort was
            // certified safe for explicit none replacement.
            if(exact.suppress_dsr_only_safe)
                return {action::suppress_dsr_only,false};
            return {action::preserve_host,false};

        case material_response::mtd_envspec_router_state::nospc_host:
            // Both renderers already use a no-Spc host. There is no DSR-only
            // EnvSpec lane to delete here.
            return {action::preserve_host,false};

        case material_response::mtd_envspec_router_state::unknown:
        default:
            return {action::preserve_host,false};
        }
    }

    // Receiver/material identity is the authority boundary for EnvSpec.
    // Never derive a positive bridge/delete decision from a weaker semantic
    // presence fallback: a non-exact query must fail open to stock DSR.
    return {action::preserve_host, false};
}

} // namespace dsrrl::operators::env_spec
