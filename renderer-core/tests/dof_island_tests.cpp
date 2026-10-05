#include "dsrrl/operators/dof/dof_island.hpp"

#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << "CHECK failed: " #x "\\n"; return 1; } } while (false)

int main()
{
    using namespace dsrrl::operators::dof;

    CHECK(ptde_flat_passes.front() == 0x00u);
    CHECK(ptde_flat_passes[1] == 0x02u);
    CHECK(dsr_flat_passes.front() == 0x01u);
    CHECK(ptde_flat_passes[2] == dsr_flat_passes[1]);
    CHECK(ptde_flat_passes.back() == 0x10u);

    CHECK(ptde_fixed_raster_ladder[0].width == 1024u);
    CHECK(ptde_fixed_raster_ladder[0].height == 720u);
    CHECK(ptde_fixed_raster_ladder[1].width == 512u);
    CHECK(ptde_fixed_raster_ladder[2].width == 256u);

    activation_context ready{};
    ready.enabled = true;
    ready.exact_imageprocess_dof_flat = true;
    ready.mode = flat_mode::primary;
    ready.graph_complete = true;
    ready.private_depth_sidecar_ready = true;
    ready.retained_plain_dofrate_ready = true;
    ready.fixed_raster_chain_ready = true;
    ready.final_composite_cut_verified = true;

    const auto active = evaluate_activation(ready);
    CHECK(active.active);
    CHECK(active.reason == bridge_reason::ready);

    auto taa_write = ready;
    taa_write.writes.taa_history = true;
    const auto taa_block = evaluate_activation(taa_write);
    CHECK(!taa_block.active);
    CHECK(taa_block.reason == bridge_reason::temporal_state_write_forbidden);

    auto velocity_write = ready;
    velocity_write.writes.velocity = true;
    const auto velocity_block = evaluate_activation(velocity_write);
    CHECK(!velocity_block.active);
    CHECK(velocity_block.reason == bridge_reason::temporal_state_write_forbidden);

    auto depth_write = ready;
    depth_write.writes.stock_native_depth = true;
    const auto depth_block = evaluate_activation(depth_write);
    CHECK(!depth_block.active);
    CHECK(depth_block.reason == bridge_reason::stock_depth_write_forbidden);

    auto missing_sidecar = ready;
    missing_sidecar.private_depth_sidecar_ready = false;
    CHECK(evaluate_activation(missing_sidecar).reason ==
          bridge_reason::missing_private_depth_sidecar);

    auto wrong_receiver = ready;
    wrong_receiver.exact_imageprocess_dof_flat = false;
    CHECK(evaluate_activation(wrong_receiver).reason ==
          bridge_reason::wrong_receiver);

    auto unknown = ready;
    unknown.mode = flat_mode::unknown;
    CHECK(evaluate_activation(unknown).reason == bridge_reason::unknown_mode);

    return 0;
}
