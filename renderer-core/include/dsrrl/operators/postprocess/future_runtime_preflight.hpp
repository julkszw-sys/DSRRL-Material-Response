#pragma once

#include "dsrrl/core/operator_catalog.hpp"
#include "dsrrl/operators/postprocess/legacy_scene_consumer_cut.hpp"

#include <cstdint>

namespace dsrrl::operators::postprocess {

enum class post_unblock_state : std::uint8_t {
    blocked = 0,
    ready_for_partial
};

enum class postprocess_sfx_scope : std::uint8_t {
    preserve_stock_dsr = 0,
    full_frame_diagnostic
};

// Production Bloom/HDR activation is bounded at the consumer semantic cut.
// Full PTDE writer-history reconstruction is a separate diagnostic scope and
// must not be silently promoted into a release prerequisite.
enum class postprocess_scene_scope : std::uint8_t {
    consumer_cut_only = 0,
    full_ptde_scene_history_diagnostic
};

enum class bloom_unblock_reason : std::uint8_t {
    ready_for_partial = 0,
    consumer_scene_cut_not_ready,
    history_writer_set_not_closed,
    history_writer_order_not_closed,
    history_draw_recurrence_not_closed,
    history_sfx_recurrence_not_closed,
    scene_domain_bridge_not_ready,
    q8_scene_source_not_ready,
    packed_depth_bridge_not_ready,
    packed_depth_quantization_not_closed,
    type06_host_not_verified,
    type06_shader_identity_not_verified,
    fixed_rgba_ladder_not_ready,
    pass04_not_ready,
    pass18_not_ready,
    pass19_1a_not_ready,
    six_pass_graph_insertion_not_ready,
    synchronization_not_ready,
    resource_ownership_not_ready,
    resource_lifetime_not_ready,
    hdr_t1_handoff_not_verified,
    lightshaft_separation_not_preserved,
    stock_sfx_graph_not_preserved,
    sfx_diagnostic_opt_in_not_ready,
    sfx_diagnostic_attribution_not_ready,
    sfx_diagnostic_release_guard_not_ready
};

struct bloom_unblock_context {
    // Release path: authenticate the PTDE-equivalent scene value at the actual
    // Bloom/HDR consumer cut. Writer provenance belongs to upstream scene
    // production and is not a Bloom-local gate.
    postprocess_scene_scope scene_scope =
        postprocess_scene_scope::consumer_cut_only;
    bool consumer_scene_cut_ready = false;

    // Diagnostic-only full-history reconstruction. These remain valuable when
    // reproducing the entire PTDE scene target, including FX/SFX recurrence,
    // but production does not require them when stock DSR SFX stays isolated.
    bool history_writer_set_closed = false;
    bool history_writer_order_closed = false;
    bool history_draw_recurrence_closed = false;
    bool history_sfx_recurrence_closed = false;
    bool scene_domain_bridge_ready = false;
    bool q8_scene_source_ready = false;

    // BrightPass depth representation.
    bool packed_depth_logical_bridge_ready = false;
    bool packed_depth_quantization_closed = false;

    // Legacy Bloom graph resources/shaders.
    bool type06_host_verified = false;
    bool type06_shader_identity_verified = false;
    bool fixed_rgba_ladder_ready = false; // 256x180 -> 128x90 A8R8G8B8 RGBA
    bool pass04_ready = false;
    bool pass18_ready = false;
    bool pass19_1a_ready = false;

    // Host integration.
    bool six_pass_graph_insertion_ready = false;
    bool synchronization_ready = false;
    bool resource_ownership_ready = false;
    bool resource_lifetime_ready = false;
    bool hdr_t1_handoff_verified = false;

    // Independent operators must remain independent.
    bool lightshaft_separation_preserved = false;

    // Production keeps the DSR-native SFX island unless separately proven.
    // A full-frame diagnostic may intentionally allow Bloom/postprocess to
    // affect SFX, but it must remain explicitly non-release and attributable.
    postprocess_sfx_scope sfx_scope =
        postprocess_sfx_scope::preserve_stock_dsr;
    bool stock_sfx_graph_preserved = false;
    bool sfx_diagnostic_opt_in = false;
    bool sfx_diagnostic_attribution_ready = false;
    bool sfx_diagnostic_non_release = false;
};

struct bloom_unblock_plan {
    post_unblock_state state = post_unblock_state::blocked;
    bloom_unblock_reason reason =
        bloom_unblock_reason::scene_domain_bridge_not_ready;

    bool direct_shader_body_swap_allowed = false;
    bool requires_q8_scene_bridge = true;
    bool requires_fixed_rgba_sidecars = true;
    bool preserve_stock_hdr_until_separately_ready = true;
    bool includes_sfx_in_postprocess = false;
    bool diagnostic_only = false;
};

inline bloom_unblock_plan evaluate_bloom_unblock_preflight(
    const bloom_unblock_context &c) noexcept
{
    bloom_unblock_plan out;
    if (!c.consumer_scene_cut_ready) {
        out.reason = bloom_unblock_reason::consumer_scene_cut_not_ready;
        return out;
    }

    if (c.scene_scope ==
        postprocess_scene_scope::full_ptde_scene_history_diagnostic) {
        if (!c.history_writer_set_closed) {
            out.reason = bloom_unblock_reason::history_writer_set_not_closed;
            return out;
        }
        if (!c.history_writer_order_closed) {
            out.reason = bloom_unblock_reason::history_writer_order_not_closed;
            return out;
        }
        if (!c.history_draw_recurrence_closed) {
            out.reason =
                bloom_unblock_reason::history_draw_recurrence_not_closed;
            return out;
        }
        if (!c.history_sfx_recurrence_closed) {
            out.reason =
                bloom_unblock_reason::history_sfx_recurrence_not_closed;
            return out;
        }
    }

    if (!c.scene_domain_bridge_ready) {
        out.reason = bloom_unblock_reason::scene_domain_bridge_not_ready;
        return out;
    }
    if (!c.q8_scene_source_ready) {
        out.reason = bloom_unblock_reason::q8_scene_source_not_ready;
        return out;
    }
    if (!c.packed_depth_logical_bridge_ready) {
        out.reason = bloom_unblock_reason::packed_depth_bridge_not_ready;
        return out;
    }
    if (!c.packed_depth_quantization_closed) {
        out.reason = bloom_unblock_reason::packed_depth_quantization_not_closed;
        return out;
    }
    if (!c.type06_host_verified) {
        out.reason = bloom_unblock_reason::type06_host_not_verified;
        return out;
    }
    if (!c.type06_shader_identity_verified) {
        out.reason = bloom_unblock_reason::type06_shader_identity_not_verified;
        return out;
    }
    if (!c.fixed_rgba_ladder_ready) {
        out.reason = bloom_unblock_reason::fixed_rgba_ladder_not_ready;
        return out;
    }
    if (!c.pass04_ready) {
        out.reason = bloom_unblock_reason::pass04_not_ready;
        return out;
    }
    if (!c.pass18_ready) {
        out.reason = bloom_unblock_reason::pass18_not_ready;
        return out;
    }
    if (!c.pass19_1a_ready) {
        out.reason = bloom_unblock_reason::pass19_1a_not_ready;
        return out;
    }
    if (!c.six_pass_graph_insertion_ready) {
        out.reason = bloom_unblock_reason::six_pass_graph_insertion_not_ready;
        return out;
    }
    if (!c.synchronization_ready) {
        out.reason = bloom_unblock_reason::synchronization_not_ready;
        return out;
    }
    if (!c.resource_ownership_ready) {
        out.reason = bloom_unblock_reason::resource_ownership_not_ready;
        return out;
    }
    if (!c.resource_lifetime_ready) {
        out.reason = bloom_unblock_reason::resource_lifetime_not_ready;
        return out;
    }
    if (!c.hdr_t1_handoff_verified) {
        out.reason = bloom_unblock_reason::hdr_t1_handoff_not_verified;
        return out;
    }
    if (!c.lightshaft_separation_preserved) {
        out.reason = bloom_unblock_reason::lightshaft_separation_not_preserved;
        return out;
    }
    if (c.sfx_scope == postprocess_sfx_scope::preserve_stock_dsr) {
        if (!c.stock_sfx_graph_preserved) {
            out.reason = bloom_unblock_reason::stock_sfx_graph_not_preserved;
            return out;
        }
    } else {
        out.includes_sfx_in_postprocess = true;
        out.diagnostic_only = true;
        if (!c.sfx_diagnostic_opt_in) {
            out.reason = bloom_unblock_reason::sfx_diagnostic_opt_in_not_ready;
            return out;
        }
        if (!c.sfx_diagnostic_attribution_ready) {
            out.reason =
                bloom_unblock_reason::sfx_diagnostic_attribution_not_ready;
            return out;
        }
        if (!c.sfx_diagnostic_non_release) {
            out.reason =
                bloom_unblock_reason::sfx_diagnostic_release_guard_not_ready;
            return out;
        }
    }

    out.state = post_unblock_state::ready_for_partial;
    out.reason = bloom_unblock_reason::ready_for_partial;
    return out;
}

enum class hdr_unblock_reason : std::uint8_t {
    ready_for_partial = 0,
    consumer_scene_cut_not_ready,
    scene_domain_bridge_not_ready,
    q8_scene_source_not_ready,
    legacy_scene_scale_lane_not_ready,
    lightshaft_lane_not_ready,
    dsr_c56w_preservation_not_ready,
    bloom_input_semantics_not_ready,
    lightshaft_input_semantics_not_ready,
    legacy_hdr_transfer_not_ready,
    ptde_output_handoff_not_closed,
    dsr_output_transfer_contract_not_ready,
    preserved_coloradjust_overlay_tail_not_ready,
    graph_insertion_not_ready,
    synchronization_not_ready,
    resource_ownership_not_ready,
    resource_lifetime_not_ready,
    output_handoff_not_verified,
    sfx_composite_order_not_verified,
    stock_dsr_sfx_not_preserved,
    sfx_inverse_tonemap_contract_not_preserved,
    sfx_diagnostic_opt_in_not_ready,
    sfx_diagnostic_attribution_not_ready,
    sfx_diagnostic_release_guard_not_ready
};

struct hdr_unblock_context {
    // Shared PTDE scene semantic cut. This is independent from the provenance
    // of upstream writers and must be validated before legacy HDR can consume
    // a Q8 sidecar.
    bool consumer_scene_cut_ready = false;

    // R24 failure must not recur: legacy HDR consumes stored PTDE scene code,
    // not the stock DSR decoded-linear HDR t0 signal.
    bool scene_domain_bridge_ready = false;
    bool q8_scene_source_ready = false;

    // Consumer-local c56 semantics only; whole-vector c56 copying is forbidden.
    bool legacy_scene_scale_lane_ready = false; // PTDE c56.z
    bool lightshaft_lane_ready = false;         // PTDE c56.y
    bool dsr_c56w_preserved = false;            // DSR adapted-luminance max

    bool bloom_input_semantics_ready = false;
    bool lightshaft_input_semantics_ready = false;
    bool legacy_hdr_transfer_ready = false;

    // Keep target and host contracts distinct. PTDE pass IDs 0x20/0x21 are
    // independently CONFIRMED CameraBlur and are not a GammaTexture/output
    // stage. PTDE's default D3D9 GammaRamp is independently CONFIRMED identity,
    // and Present is known, but the exact object-level HDR_ColAdj -> backbuffer
    // render-target handoff remains OPEN. Keep that resource-plumbing proof as
    // a separate gate instead of inventing an extra transfer operator.
    bool ptde_output_handoff_closed = false;

    // R24 proved that dropping DSR's explicit ~pow(1/2.2) output transfer on a
    // non-sRGB swapchain violates the host output contract. A legacy HDR port
    // must therefore provide an explicit PTDE-result -> DSR backbuffer transfer
    // contract rather than assuming the retained legacy body is final-output ready.
    bool dsr_output_transfer_contract_ready = false;
    // PTDE and DSR ordinary ToneCorrect/ColorAdjust tails are operator-
    // equivalent for the shared fields. Do not rebuild or retune this tail as
    // part of HDR; preserve the routed DSR tail and bridge only the upstream
    // legacy HDR composition plus the host output contract.
    bool preserved_coloradjust_overlay_tail_ready = false;

    bool graph_insertion_ready = false;
    bool synchronization_ready = false;
    bool resource_ownership_ready = false;
    bool resource_lifetime_ready = false;
    bool output_handoff_verified = false;

    // Stock DSR spells/VFX remain a host-owned island. Any HDR replacement
    // must preserve their composition domain unless that island is redesigned.
    bool sfx_composite_order_verified = false;

    postprocess_sfx_scope sfx_scope =
        postprocess_sfx_scope::preserve_stock_dsr;
    bool stock_dsr_sfx_preserved = false;
    bool sfx_inverse_tonemap_contract_preserved = false;
    bool sfx_diagnostic_opt_in = false;
    bool sfx_diagnostic_attribution_ready = false;
    bool sfx_diagnostic_non_release = false;
};

struct hdr_unblock_plan {
    post_unblock_state state = post_unblock_state::blocked;
    hdr_unblock_reason reason =
        hdr_unblock_reason::scene_domain_bridge_not_ready;

    bool direct_legacy_body_swap_allowed = false;
    bool whole_c56_copy_allowed = false;
    bool requires_q8_scene_bridge = true;
    bool preserve_native_sfx_island = true;
    bool includes_sfx_in_postprocess = false;
    bool diagnostic_only = false;
};

inline hdr_unblock_plan evaluate_hdr_unblock_preflight(
    const hdr_unblock_context &c) noexcept
{
    hdr_unblock_plan out;
    if (!c.consumer_scene_cut_ready) {
        out.reason = hdr_unblock_reason::consumer_scene_cut_not_ready;
        return out;
    }
    if (!c.scene_domain_bridge_ready) {
        out.reason = hdr_unblock_reason::scene_domain_bridge_not_ready;
        return out;
    }
    if (!c.q8_scene_source_ready) {
        out.reason = hdr_unblock_reason::q8_scene_source_not_ready;
        return out;
    }
    if (!c.legacy_scene_scale_lane_ready) {
        out.reason = hdr_unblock_reason::legacy_scene_scale_lane_not_ready;
        return out;
    }
    if (!c.lightshaft_lane_ready) {
        out.reason = hdr_unblock_reason::lightshaft_lane_not_ready;
        return out;
    }
    if (!c.dsr_c56w_preserved) {
        out.reason = hdr_unblock_reason::dsr_c56w_preservation_not_ready;
        return out;
    }
    if (!c.bloom_input_semantics_ready) {
        out.reason = hdr_unblock_reason::bloom_input_semantics_not_ready;
        return out;
    }
    if (!c.lightshaft_input_semantics_ready) {
        out.reason = hdr_unblock_reason::lightshaft_input_semantics_not_ready;
        return out;
    }
    if (!c.legacy_hdr_transfer_ready) {
        out.reason = hdr_unblock_reason::legacy_hdr_transfer_not_ready;
        return out;
    }
    if (!c.ptde_output_handoff_closed) {
        out.reason = hdr_unblock_reason::ptde_output_handoff_not_closed;
        return out;
    }
    if (!c.dsr_output_transfer_contract_ready) {
        out.reason = hdr_unblock_reason::dsr_output_transfer_contract_not_ready;
        return out;
    }
    if (!c.preserved_coloradjust_overlay_tail_ready) {
        out.reason = hdr_unblock_reason::preserved_coloradjust_overlay_tail_not_ready;
        return out;
    }
    if (!c.graph_insertion_ready) {
        out.reason = hdr_unblock_reason::graph_insertion_not_ready;
        return out;
    }
    if (!c.synchronization_ready) {
        out.reason = hdr_unblock_reason::synchronization_not_ready;
        return out;
    }
    if (!c.resource_ownership_ready) {
        out.reason = hdr_unblock_reason::resource_ownership_not_ready;
        return out;
    }
    if (!c.resource_lifetime_ready) {
        out.reason = hdr_unblock_reason::resource_lifetime_not_ready;
        return out;
    }
    if (!c.output_handoff_verified) {
        out.reason = hdr_unblock_reason::output_handoff_not_verified;
        return out;
    }
    if (!c.sfx_composite_order_verified) {
        out.reason = hdr_unblock_reason::sfx_composite_order_not_verified;
        return out;
    }
    if (c.sfx_scope == postprocess_sfx_scope::preserve_stock_dsr) {
        if (!c.stock_dsr_sfx_preserved) {
            out.reason = hdr_unblock_reason::stock_dsr_sfx_not_preserved;
            return out;
        }
        if (!c.sfx_inverse_tonemap_contract_preserved) {
            out.reason =
                hdr_unblock_reason::sfx_inverse_tonemap_contract_not_preserved;
            return out;
        }
    } else {
        out.preserve_native_sfx_island = false;
        out.includes_sfx_in_postprocess = true;
        out.diagnostic_only = true;
        if (!c.sfx_diagnostic_opt_in) {
            out.reason = hdr_unblock_reason::sfx_diagnostic_opt_in_not_ready;
            return out;
        }
        if (!c.sfx_diagnostic_attribution_ready) {
            out.reason =
                hdr_unblock_reason::sfx_diagnostic_attribution_not_ready;
            return out;
        }
        if (!c.sfx_diagnostic_non_release) {
            out.reason =
                hdr_unblock_reason::sfx_diagnostic_release_guard_not_ready;
            return out;
        }
    }

    out.state = post_unblock_state::ready_for_partial;
    out.reason = hdr_unblock_reason::ready_for_partial;
    return out;
}

} // namespace dsrrl::operators::postprocess
