#include "dsrrl/operators/dof/dof_island.hpp"

#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << "CHECK failed: " #x "\n"; return 1; } } while (false)

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

    CHECK(retained_pipeline_signatures.size() == 16u);
    CHECK(retained_runtime_id_sequence_is_contiguous());
    CHECK(retained_pipeline_signatures[
        static_cast<std::size_t>(retained_shader_role::dof_rate_plain)]
        .runtime_shader_id == 0x0DEFu);
    CHECK(retained_pipeline_signatures[
        static_cast<std::size_t>(retained_shader_role::dof_rate_cb)]
        .runtime_shader_id == 0x0DF0u);

    const auto &gx = retained_pipeline_signatures[
        static_cast<std::size_t>(retained_shader_role::gauss_x)];
    const auto &gy = retained_pipeline_signatures[
        static_cast<std::size_t>(retained_shader_role::gauss_y)];
    CHECK(gx.pixel_sha256 == gy.pixel_sha256);
    CHECK(gx.pixel_size == gy.pixel_size);
    CHECK(!(gx.vertex_sha256 == gy.vertex_sha256));
    CHECK(find_retained_pipeline(
        gx.vertex_sha256, gx.vertex_size,
        gx.pixel_sha256, gx.pixel_size) == &gx);
    CHECK(find_retained_pipeline(
        gy.vertex_sha256, gy.vertex_size,
        gy.pixel_sha256, gy.pixel_size) == &gy);

    CHECK(dsr_flat_retained_routes[4].pass == 0x0Du);
    CHECK(dsr_flat_retained_routes[4].primary ==
          retained_shader_role::dof_rate_cb);
    CHECK(dsr_flat_retained_routes.back().pass == 0x10u);
    CHECK(dsr_flat_retained_routes.back().primary ==
          retained_shader_role::gauss_y_adv);
    CHECK(dsr_flat_retained_routes.back().alternate ==
          retained_shader_role::near_rate);
    CHECK(!flat_pass_0x10_is_output_composite());
    CHECK(active_output_cut_is_exact());
    CHECK(dsr_active_output_cut.image_filter_ctor ==
          0x140450D30ull);
    CHECK(dsr_active_output_cut.tonemap_ctor ==
          0x140461A70ull);
    CHECK(dsr_active_output_cut.scene_pass_builder ==
          0x140452ED0ull);
    CHECK(dsr_active_output_cut.image_state_target_offset == 0xF8u);
    CHECK(dsr_active_output_cut.image_state_srv_alias_offset == 0x104u);
    CHECK(dsr_active_output_cut.tonemap_pass == 0x13u);
    CHECK(dsr_active_output_cut.builder_primary_srv_argument == 6u);

    activation_context ready{};
    ready.enabled = true;
    ready.exact_imageprocess_dof_flat = true;
    ready.mode = flat_mode::primary;
    ready.graph_complete = true;
    ready.ptde_dofbank_payload_ready = true;
    ready.ptde_dofbank_route_verified = true;
    ready.retained_flat_pipeline_set_ready = true;
    ready.private_depth_sidecar_ready = true;
    ready.retained_plain_dofrate_ready = true;
    ready.fixed_raster_chain_ready = true;
    ready.output_cut_verified = true;

    const auto active = evaluate_activation(ready);
    CHECK(active.active);
    CHECK(active.reason == bridge_reason::ready);

    auto missing_bank_payload = ready;
    missing_bank_payload.ptde_dofbank_payload_ready = false;
    CHECK(evaluate_activation(missing_bank_payload).reason ==
          bridge_reason::missing_ptde_dofbank_payload);

    auto unresolved_bank_route = ready;
    unresolved_bank_route.ptde_dofbank_route_verified = false;
    CHECK(evaluate_activation(unresolved_bank_route).reason ==
          bridge_reason::unresolved_ptde_dofbank_route);

    auto missing_pipelines = ready;
    missing_pipelines.retained_flat_pipeline_set_ready = false;
    CHECK(evaluate_activation(missing_pipelines).reason ==
          bridge_reason::missing_retained_pipeline_set);

    auto taa_write = ready;
    taa_write.writes.taa_history = true;
    CHECK(evaluate_activation(taa_write).reason ==
          bridge_reason::temporal_state_write_forbidden);

    auto velocity_write = ready;
    velocity_write.writes.velocity = true;
    CHECK(evaluate_activation(velocity_write).reason ==
          bridge_reason::temporal_state_write_forbidden);

    auto depth_write = ready;
    depth_write.writes.stock_native_depth = true;
    CHECK(evaluate_activation(depth_write).reason ==
          bridge_reason::stock_depth_write_forbidden);

    auto missing_sidecar = ready;
    missing_sidecar.private_depth_sidecar_ready = false;
    CHECK(evaluate_activation(missing_sidecar).reason ==
          bridge_reason::missing_private_depth_sidecar);

    auto missing_output = ready;
    missing_output.output_cut_verified = false;
    CHECK(evaluate_activation(missing_output).reason ==
          bridge_reason::missing_output_cut);

    return 0;
}
