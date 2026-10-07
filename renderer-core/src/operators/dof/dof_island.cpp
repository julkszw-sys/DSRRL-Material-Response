#include "dsrrl/operators/dof/dof_island.hpp"

namespace dsrrl::operators::dof {

activation_decision evaluate_activation(const activation_context &context) noexcept
{
    if (!context.enabled)
        return {false, bridge_reason::disabled};

    if (!context.exact_imageprocess_dof_flat)
        return {false, bridge_reason::wrong_receiver};

    if (context.mode == flat_mode::unknown)
        return {false, bridge_reason::unknown_mode};

    if (!preserves_temporal_state(context.writes))
        return {false, bridge_reason::temporal_state_write_forbidden};

    if (!preserves_stock_depth(context.writes))
        return {false, bridge_reason::stock_depth_write_forbidden};

    if (!context.graph_complete)
        return {false, bridge_reason::incomplete_graph};

    if (!context.ptde_dofbank_payload_ready)
        return {false, bridge_reason::missing_ptde_dofbank_payload};

    if (!context.ptde_dofbank_route_verified)
        return {false, bridge_reason::unresolved_ptde_dofbank_route};

    if (context.carrier == carrier_mode::unknown)
        return {false, bridge_reason::unknown_carrier};

    if (context.carrier == carrier_mode::exact_q8_history &&
        !context.q8_scene_history_ready)
        return {false, bridge_reason::missing_q8_scene_history};

    if (context.carrier == carrier_mode::native_rate_half_seed &&
        !context.half_seed_adapter_ready)
        return {false, bridge_reason::missing_half_seed_adapter};

    if (!context.pass_state_transaction_ready)
        return {false, bridge_reason::missing_pass_state_transaction};

    if (!context.retained_flat_pipeline_set_ready)
        return {false, bridge_reason::missing_retained_pipeline_set};

    if (!context.private_depth_sidecar_ready)
        return {false, bridge_reason::missing_private_depth_sidecar};

    if (!context.retained_plain_dofrate_ready)
        return {false, bridge_reason::missing_plain_dofrate};

    if (!context.fixed_raster_chain_ready)
        return {false, bridge_reason::missing_fixed_raster_chain};

    if (!context.tonemap_dof_continuation_verified)
        return {false, bridge_reason::incomplete_tonemap_dof_continuation};

    if (!context.output_cut_verified)
        return {false, bridge_reason::missing_output_cut};

    return {true, bridge_reason::ready};
}

} // namespace dsrrl::operators::dof
