#include "dsrrl/operators/env_spec/no_spc_envspec_delete.hpp"

#include <cmath>

namespace dsrrl::operators::env_spec {

no_spc_delete_decision evaluate_no_spc_envspec_delete(
    float dsr_envspec_term,
    const no_spc_delete_context &context) noexcept
{
    if (!std::isfinite(dsr_envspec_term))
        return {
            no_spc_delete_action::preserve_host,
            no_spc_delete_reason::nonfinite_host_term,
            dsr_envspec_term,
            dsr_envspec_term
        };

    if (!context.exact_homolog_verified)
        return {
            no_spc_delete_action::preserve_host,
            no_spc_delete_reason::homolog_not_verified,
            dsr_envspec_term,
            dsr_envspec_term
        };

    if (!context.substantive_pbl_no_spc)
        return {
            no_spc_delete_action::preserve_host,
            no_spc_delete_reason::wrong_receiver_class,
            dsr_envspec_term,
            dsr_envspec_term
        };

    if (!context.ptde_envspec_lane_absent)
        return {
            no_spc_delete_action::preserve_host,
            no_spc_delete_reason::ptde_lane_not_proven_absent,
            dsr_envspec_term,
            dsr_envspec_term
        };

    if (!context.alias_scope_safe)
        return {
            no_spc_delete_action::preserve_host,
            no_spc_delete_reason::alias_scope_not_safe,
            dsr_envspec_term,
            dsr_envspec_term
        };

    return {
        no_spc_delete_action::delete_dsr_only_envspec,
        no_spc_delete_reason::exact_certified,
        dsr_envspec_term,
        0.0f
    };
}

} // namespace dsrrl::operators::env_spec
