#include "dsrrl/runtime/subsurface_draw_runtime.hpp"
#include "dsrrl/runtime/subsurface_pipeline_registry.hpp"
#include "dsrrl/operators/resource_bridges/subsurface_route.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

namespace dsrrl::runtime {
namespace {

constexpr std::uint32_t k_ptde_body_route_index = 232u;

const operators::resource_bridges::subsurface_receiver_route *
route_for_target(std::uint32_t target_receiver) noexcept
{
    for (const auto &route :
         operators::resource_bridges::
             k_subsurface_receiver_routes) {
        if (route.target_plain_receiver_id ==
            target_receiver)
            return &route;
    }

    return nullptr;
}

bool exact_dsbt_material(
    const operators::material_response::material_identity &material) noexcept
{
    namespace mr = operators::material_response;
    namespace subs =
        operators::resource_bridges;

    return
        material.valid &&
        material.owner_tuple_exact &&
        material.material_slot_valid &&
        material.semantic_name_hash ==
            mr::mtd_semantic_hash(
                subs::k_dsr_body_subsurf_material) &&
        operators::legacy_plan::hashing::
            matches_hex(
                material.raw_mtd_sha256,
                subs::k_dsr_body_subsurf_material_sha256);
}

} // namespace

subsurface_draw_runtime::subsurface_draw_runtime(
    core::renderer_core &core,
    material_response_draw_runtime &mr,
    material_resource_draw_runtime &resources) noexcept
    : core_(core),
      mr_(mr),
      resources_(resources)
{
}

bool subsurface_draw_runtime::prepare(
    reshade::api::command_list *cmd_list,
    const operators::material_response::material_identity &material,
    prepared_subsurface_draw &prepared) noexcept
{
    prepared = {};

    if (cmd_list == nullptr ||
        !core_.features().enabled(
            core::operator_id::subsurface))
        return false;

    ++candidates_;

    std::uint32_t target_receiver = 0u;
    if (!subsurface_receiver_bound(
            cmd_list,
            target_receiver)) {
        ++pipeline_rejects_;
        return false;
    }

    const auto *route =
        route_for_target(target_receiver);
    if (route == nullptr) {
        ++pipeline_rejects_;
        return false;
    }

    if (!exact_dsbt_material(material)) {
        ++material_rejects_;
        return false;
    }

    if (!mr_.prepare_prevalidated_route_request(
            target_receiver,
            k_ptde_body_route_index,
            prepared.mr)) {
        ++surface_rejects_;
        return false;
    }

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());

    operators::resource_bridges::
        subsurface_body_texture body_texture =
            operators::resource_bridges::
                subsurface_body_texture::unknown;

    if (!resources_.prepare_subsurface_body_requests(
            context,
            target_receiver,
            prepared.resources,
            body_texture)) {
        mr_.release_prepared_draw(
            prepared.mr);
        ++surface_rejects_;
        return false;
    }

    operators::resource_bridges::
        subsurface_route_context route_context{};

    route_context.actual_material_verified = true;
    route_context.actual_material_name =
        operators::resource_bridges::
            k_dsr_body_subsurf_material;
    route_context.actual_material_sha256 =
        operators::resource_bridges::
            k_dsr_body_subsurf_material_sha256;

    route_context.actual_receiver_verified = true;
    route_context.actual_receiver_name =
        route->dsr_receiver_name;
    route_context.actual_receiver_sha256 =
        route->dsr_receiver_sha256;

    route_context.actual_body_texture_verified =
        body_texture !=
            operators::resource_bridges::
                subsurface_body_texture::unknown;
    route_context.body_spec_texture =
        body_texture;

    route_context.stable_hemenv_no_pointlight_draw_verified =
        true;

    route_context.ptde_slot_mapping_verified = true;
    route_context.ptde_donor_verified = true;
    route_context.ptde_material_name =
        operators::resource_bridges::
            k_ptde_body_plain_material;
    route_context.ptde_material_sha256 =
        operators::resource_bridges::
            k_ptde_body_plain_material_sha256;

    route_context.ptde_subsurface_usage_verified = true;
    route_context.ptde_uses_subsurface = false;
    route_context.ptde_plain_surface_target_verified = true;

    route_context.target_plain_receiver_ready =
        prepared.mr.ready;
    route_context.spec_rgb_route_ready =
        prepared.resources.spec_rgb;
    route_context.diffuse_route_ready =
        prepared.resources.diffuse;
    route_context.normal_route_ready =
        prepared.resources.normal;
    route_context.material_response_route_ready =
        prepared.mr.ready;

    route_context.dsr_subsurf_bypass_carrier_ready =
        prepared.mr.shader != nullptr;

    const auto decision =
        operators::resource_bridges::
            evaluate_subsurface_route(
                route_context);

    if (decision.action !=
            operators::resource_bridges::
                subsurface_route_action::
                    route_to_ptde_plain_difspcbmp_surface ||
        decision.target_plain_receiver_id !=
            target_receiver ||
        decision.carrier !=
            operators::resource_bridges::
                subsurface_bypass_carrier::
                    create_time_pixel_shader_substitution) {
        release(prepared);
        ++surface_rejects_;
        return false;
    }

    prepared.subsurface = {};
    prepared.subsurface.primary =
        core::operator_id::subsurface;
    prepared.subsurface.receiver_verified = true;
    prepared.subsurface.material_verified = true;
    prepared.subsurface.pixel_shader =
        prepared.mr.shader;
    prepared.subsurface.replace_pixel_shader = true;

    draw_tx_mutation verify{};
    if (build_island_draw_mutation(
            prepared.subsurface,
            verify) !=
        island_draw_adapter_result::ready) {
        release(prepared);
        ++surface_rejects_;
        return false;
    }

    prepared.target_receiver_id =
        target_receiver;
    prepared.ready = true;
    ++prepared_;
    return true;
}

void subsurface_draw_runtime::release(
    prepared_subsurface_draw &prepared) noexcept
{
    resources_.release_prepared_draw(
        prepared.resources);
    mr_.release_prepared_draw(
        prepared.mr);
    prepared = {};
}

subsurface_draw_telemetry
subsurface_draw_runtime::telemetry() const noexcept
{
    return {
        candidates_.load(),
        material_rejects_.load(),
        pipeline_rejects_.load(),
        surface_rejects_.load(),
        prepared_.load()
    };
}

void subsurface_draw_runtime::reset() noexcept
{
    candidates_.store(0);
    material_rejects_.store(0);
    pipeline_rejects_.store(0);
    surface_rejects_.store(0);
    prepared_.store(0);
}

} // namespace dsrrl::runtime
