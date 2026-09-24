#pragma once

#include <cstdint>

namespace dsrrl::operators::env_spec {

enum class no_spc_delete_action : std::uint8_t {
    preserve_host = 0,
    delete_dsr_only_envspec
};

enum class no_spc_delete_reason : std::uint8_t {
    exact_certified = 0,
    nonfinite_host_term,
    homolog_not_verified,
    wrong_receiver_class,
    ptde_lane_not_proven_absent,
    alias_scope_not_safe
};

struct no_spc_delete_context {
    bool exact_homolog_verified = false;
    bool substantive_pbl_no_spc = false;
    bool ptde_envspec_lane_absent = false;
    bool alias_scope_safe = false;
};

struct no_spc_delete_decision {
    no_spc_delete_action action = no_spc_delete_action::preserve_host;
    no_spc_delete_reason reason = no_spc_delete_reason::homolog_not_verified;
    float stock_dsr_envspec_term = 0.0f;
    float bridge_envspec_term = 0.0f;
};

// Exact local semantic deletion for the certified 48 substantive PBL no-Spc
// homologs. Every identity/homology/alias guard must be true; otherwise the
// host contribution is preserved.
//
// Certified target: EnvSpec_term := 0
no_spc_delete_decision evaluate_no_spc_envspec_delete(
    float dsr_envspec_term,
    const no_spc_delete_context &context) noexcept;

} // namespace dsrrl::operators::env_spec
