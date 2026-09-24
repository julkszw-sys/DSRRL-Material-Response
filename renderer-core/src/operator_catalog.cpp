#include "dsrrl/core/operator_catalog.hpp"

#include <cstring>

namespace dsrrl::core {
namespace {

constexpr std::array<operator_contract, operator_count> k_catalog = {{
    {operator_id::material_response, "bridge.material_response", "renderer.core.islands.phase1.material_response_envspec_gate_v1", "Material Response route island", canonical_status::confirmed, port_state::off, carrier_kind::hybrid, require_receiver | require_consumer, true},
    {operator_id::upper_lower, "surface.upper_lower", "project.branch.renderer_edition_ul_ptde_operator_port_contract", "Upper/Lower Ambient", canonical_status::confirmed, port_state::off, carrier_kind::constant_buffer, require_receiver | require_producer | require_consumer, true},
    {operator_id::hemdir3, "surface.hemdir3", "project.branch.renderer_edition_hemdir3_source_join_contract", "HemDir3 D1/D2/D3 + U/L source", canonical_status::confirmed, port_state::partial, carrier_kind::constant_buffer, require_receiver | require_producer | require_consumer, true},
    {operator_id::spec_rgb, "bridge.specrgb", "project.materialization.ptde_specrgb_bridge_full24_stable_hemenv_v1", "PTDE SpecRGB split-resource bridge", canonical_status::confirmed, port_state::off, carrier_kind::resource, require_receiver | require_material | require_resource | require_consumer, true},
    {operator_id::env_spec, "surface.envspec_legacy", "project.branch.renderer_edition_envspec_exact_legacy_bridge_contract_v1", "EnvSpec exact legacy bridge", canonical_status::confirmed, port_state::partial, carrier_kind::hybrid, require_receiver | require_material | require_resource | require_consumer, true},
    {operator_id::envspec_nospc_delete, "surface.envspec_nospc_delete", "renderer.cross.hemenv_shaderonly_bridge_census_v1", "No-Spc EnvSpec deletion", canonical_status::confirmed, port_state::active_candidate, carrier_kind::shader, require_receiver | require_consumer, true},
    {operator_id::envspec_pmetal_diagnostic, "surface.envspec_pmetal", "project.branch.renderer_edition_r06_pmetal_ptde_envspec_receiver_construction", "P_Metal fixed-LOD EnvSpec proxy", canonical_status::confirmed, port_state::diagnostic, carrier_kind::hybrid, require_receiver | require_material | require_resource | require_consumer, true},
    {operator_id::env_diffuse, "surface.envdiffuse", "renderer.core.islands.envdiffuse_narrow_carrier_v1", "EnvDiffuse split source+probe bridge", canonical_status::high_confidence, port_state::partial, carrier_kind::hybrid, require_receiver | require_resource | require_producer | require_consumer, true},
    {operator_id::point_light, "surface.pointlight_full", "renderer.ptde.pointlight.end_to_end_forward_v1", "PTDE PointLight full causal forward", canonical_status::high_confidence, port_state::partial, carrier_kind::hybrid, require_receiver | require_producer | require_consumer, true},
    {operator_id::pointlight_pnts_attenuation, "surface.pointlight_pnts_attenuation", "renderer.cross.pointlight.pnts_linear72_shader_translation_v1", "PntS attenuation bridge", canonical_status::confirmed, port_state::active_candidate, carrier_kind::shader, require_receiver | require_consumer, true},
    {operator_id::local_specular_legacy, "surface.local_specular_legacy", "renderer.cross.pointlight.ptde_specular_branch_output_cut", "Legacy local PointLight specular", canonical_status::confirmed, port_state::partial, carrier_kind::hybrid, require_receiver | require_material | require_producer | require_consumer, true},
    {operator_id::subsurface, "bridge.subsurface", "renderer.core.islands.subsurface_plain_route_contract_v1", "PTDE Subsurface route island", canonical_status::high_confidence, port_state::active_candidate, carrier_kind::hybrid, require_receiver | require_material | require_resource | require_consumer, true},
    {operator_id::diffuse, "bridge.diffuse_resource", "project.renderer.equipment_diffuse_bridge_architecture_v1", "PTDE Diffuse resource bridge", canonical_status::confirmed, port_state::off, carrier_kind::hybrid, require_receiver | require_material | require_resource | require_consumer, true},
    {operator_id::normal, "bridge.normal_resource", "project.renderer.equipment_normal_material_route_gate_v1", "PTDE Normal resource bridge", canonical_status::confirmed, port_state::off, carrier_kind::resource, require_receiver | require_material | require_resource | require_consumer, true},
    {operator_id::diffuse_material_domain, "surface.diffuse_material_domain", "renderer.cross.hemenv_fixed_phn_material_domain_family_census", "Diffuse material-domain bridge", canonical_status::confirmed, port_state::active_candidate, carrier_kind::shader, require_receiver | require_consumer, true},
    {operator_id::terminal_sat_rgb, "surface.terminal_sat_rgb", "renderer.cross.phn_non_faceeye_terminal_sat_bridge_v1", "Terminal RGB SAT", canonical_status::confirmed, port_state::active_candidate, carrier_kind::shader, require_receiver | require_consumer, true},
    {operator_id::terminal_sat_rgba, "surface.terminal_sat_rgba", "renderer.diagnostic.combined_rgba_terminal_sat_expansion_candidate", "Terminal RGBA SAT expansion", canonical_status::rejected, port_state::blocked, carrier_kind::shader, require_receiver | require_consumer, true},
    {operator_id::fixed_postfog_identity, "surface.fixed_postfog_identity", "renderer.cross.hemenv.fixed_postfog_identity_shader_bridge_v1", "Fixed-family post-Fog identity", canonical_status::confirmed, port_state::active_candidate, carrier_kind::shader, require_receiver | require_consumer, true},
    {operator_id::faceeye_shadow_legacy, "surface.faceeye_shadow_legacy", "renderer.core.islands.faceeye_shadow_resource_sampler_boundary_v1", "PTDE FaceEye legacy shadow/environment operator", canonical_status::confirmed, port_state::partial, carrier_kind::hybrid, require_receiver | require_resource | require_consumer, true},
    {operator_id::post_bloom, "post.bloom", "project.branch.renderer_edition_bloom_portability_status", "Bloom graph sidecar", canonical_status::confirmed, port_state::blocked, carrier_kind::composite, require_graph | require_resource | require_consumer, true},
    {operator_id::post_hdr, "post.hdr", "project.branch.renderer_edition_hdr_r24_input_scene_domain_mismatch", "HDR semantic bridge", canonical_status::confirmed, port_state::blocked, carrier_kind::composite, require_graph | require_resource | require_consumer, true},
    {operator_id::dsr_native_sfx, "dsr_native_sfx_island", "renderer.bridge.dsr_native_sfx_island_multigate_boundary_v1", "DSR Native Spell / VFX Island", canonical_status::high_confidence, port_state::stock_host, carrier_kind::host_preserve, require_none, true},
    {operator_id::dsr_sfx_inverse_tonemap, "dsr.sfxpbl.inverse_tonemap", "renderer.dsr.sfxpbl.dedicated_inverse_tonemap_operator_v1", "DSR dedicated SfxPBL inverse-tonemap", canonical_status::confirmed, port_state::stock_host, carrier_kind::host_preserve, require_none, true},
    {operator_id::pmetal_black_safe_source, "surface.pmetal_black_safe_source", "project.materialization.pmetal_envspec_v13_ptde_donor_ab_v1", "P_Metal V13 black-safe source", canonical_status::confirmed, port_state::off, carrier_kind::hybrid, require_receiver | require_material | require_resource | require_producer | require_consumer, true},
    {operator_id::pmetal_black_safe_v10, "surface.pmetal_black_safe_v10", "project.runtime.pmetal_blackspot_v10_paired_lerp_fix_v1", "P_Metal V10 black-safe paired Lerp", canonical_status::confirmed, port_state::active_candidate, carrier_kind::shader, require_receiver | require_material | require_consumer, true},
}};
static_assert(k_catalog.size() == operator_count, "Every operator_id must have one catalog contract.");

} // namespace

const std::array<operator_contract, operator_count> &known_operator_catalog() noexcept
{
    return k_catalog;
}

std::optional<operator_contract> find_operator_contract(operator_id id) noexcept
{
    const auto index = static_cast<std::size_t>(id);
    if (index >= k_catalog.size())
        return std::nullopt;
    return k_catalog[index];
}

std::optional<operator_contract> find_operator_contract(const char *operator_key) noexcept
{
    if (operator_key == nullptr)
        return std::nullopt;

    for (const auto &entry : k_catalog)
        if (std::strcmp(entry.operator_key, operator_key) == 0)
            return entry;

    return std::nullopt;
}

} // namespace dsrrl::core
