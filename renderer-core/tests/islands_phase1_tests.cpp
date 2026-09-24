#include "dsrrl/operators/env_spec/env_spec_island.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/material_response_seed.hpp"
#include "dsrrl/core/island_policy.hpp"
#include "dsrrl/core/operator_catalog.hpp"
#include "dsrrl/operators/legacy_plan/a1_mask_decomposition.hpp"
#include "dsrrl/operators/legacy_plan/generated_a1_plan_index_v1.hpp"
#include "dsrrl/operators/lightbank/lightbank_islands.hpp"
#include "dsrrl/operators/point_light/point_light_islands.hpp"
#include "dsrrl/operators/postprocess/postprocess_islands.hpp"
#include "dsrrl/operators/resource_bridges/resource_bridge_islands.hpp"
#include "dsrrl/operators/sfx/sfx_islands.hpp"
#include "dsrrl/operators/surface/surface_shader_islands.hpp"

#include <array>
#include <cstring>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>

using namespace dsrrl;
using namespace dsrrl::operators::material_response;

namespace {

bool check(bool condition, const char *expression, int line)
{
    if (condition)
        return true;

    std::cerr << "CHECK FAILED line " << line << ": " << expression << '\n';
    return false;
}

#define CHECK(expr) do { if (!check(static_cast<bool>(expr), #expr, __LINE__)) return 1; } while (false)

core::sha256_digest digest(std::uint8_t seed)
{
    core::sha256_digest out{};
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<std::uint8_t>(seed + i);
    return out;
}

void write_u32(std::array<std::uint8_t, 16> &bytes, std::size_t offset, std::uint32_t value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::uint32_t read_u32(const std::array<std::uint8_t, 16> &bytes, std::size_t offset)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

} // namespace

int main()
{
    material_response_island mr;

    CHECK(mr.register_receiver_recipe(receiver_recipe{
        500,
        material_scope_policy::global_receiver_safe,
        diffuse_material_domain_linear,
        ptde_envspec_presence::absent
    }));

    auto d = mr.evaluate(500, std::nullopt);
    CHECK(d.active);
    CHECK(d.certified_operations == diffuse_material_domain_linear);
    CHECK(d.envspec == ptde_envspec_presence::absent);

    auto env = operators::env_spec::env_spec_island::gate(d.envspec, false);
    CHECK(env.selected == operators::env_spec::action::suppress_dsr_only);
    CHECK(!env.ptde_bridge_required);

    CHECK(mr.register_receiver_recipe(receiver_recipe{
        33,
        material_scope_policy::exact_material_required,
        diffuse_material_domain_linear | specular_factor_c101,
        ptde_envspec_presence::unknown
    }));

    d = mr.evaluate(33, std::nullopt);
    CHECK(!d.active);
    CHECK(d.reason == decision_reason::material_required);

    material_profile pmetal;
    pmetal.route_index = 345;
    pmetal.semantic_name_hash = 0x504D4554414C4453ull;
    pmetal.raw_mtd_sha256 = digest(10);
    pmetal.material_family_hash = 0xD1F5C0B0ull;
    pmetal.c101 = 2.5f;
    pmetal.lod_min = 3;
    pmetal.lod_max = 4;
    pmetal.receiver_ids = {33, 34, 35, 0};
    pmetal.receiver_count = 3;
    pmetal.certified_operations =
        diffuse_material_domain_linear | specular_factor_c101;
    pmetal.envspec = ptde_envspec_presence::present;
    pmetal.semantic_name_required = true;
    CHECK(mr.register_material_profile(pmetal));

    material_identity pmetal_id;
    pmetal_id.valid = true;
    pmetal_id.route_index = 345;
    pmetal_id.semantic_name_hash = pmetal.semantic_name_hash;
    pmetal_id.raw_mtd_sha256 = pmetal.raw_mtd_sha256;
    pmetal_id.material_family_hash = pmetal.material_family_hash;

    d = mr.evaluate(33, pmetal_id);
    CHECK(d.active);
    CHECK(d.route_index == 345);
    CHECK(d.c101 == 2.5f);
    CHECK(d.lod_min == 3 && d.lod_max == 4);
    CHECK(d.envspec == ptde_envspec_presence::present);

    env = operators::env_spec::env_spec_island::gate(d.envspec, false);
    CHECK(env.selected == operators::env_spec::action::preserve_host);
    CHECK(env.ptde_bridge_required);

    env = operators::env_spec::env_spec_island::gate(d.envspec, true);
    CHECK(env.selected == operators::env_spec::action::activate_ptde_bridge);

    material_profile edge_p = pmetal;
    edge_p.route_index = 5;
    edge_p.semantic_name_hash = 0x504D4554414C4544ull;
    edge_p.raw_mtd_sha256 = digest(40);
    edge_p.envspec = ptde_envspec_presence::unknown;
    edge_p.semantic_name_required = true;
    CHECK(mr.register_material_profile(edge_p));

    material_profile edge_s = edge_p;
    edge_s.semantic_name_hash = 0x534D4554414C4544ull;
    CHECK(mr.register_material_profile(edge_s));

    material_identity ambiguous;
    ambiguous.valid = true;
    ambiguous.route_index = 5;
    ambiguous.raw_mtd_sha256 = digest(40);
    ambiguous.material_family_hash = edge_p.material_family_hash;

    d = mr.evaluate(33, ambiguous);
    CHECK(!d.active);
    CHECK(d.reason == decision_reason::unknown_material);

    ambiguous.semantic_name_hash = edge_s.semantic_name_hash;
    d = mr.evaluate(33, ambiguous);
    CHECK(d.active);
    CHECK(d.route_index == 5);

    env = operators::env_spec::env_spec_island::gate(
        ptde_envspec_presence::unknown,
        false);
    CHECK(env.selected == operators::env_spec::action::preserve_host);

    material_response_island seeded;
    CHECK(register_confirmed_material_routes_v1(seeded) == 35);
    CHECK(seeded.material_profile_count() == 35);

    const auto &catalog = core::known_operator_catalog();
    CHECK(catalog.size() == core::operator_count);
    CHECK(core::operator_count == 23);

    for (std::size_t i = 0; i < catalog.size(); ++i) {
        CHECK(static_cast<std::size_t>(catalog[i].id) == i);
        CHECK(catalog[i].operator_key != nullptr);
        CHECK(catalog[i].canonical_key != nullptr);
        CHECK(catalog[i].display_name != nullptr);
        CHECK(catalog[i].fail_open_stock);
    }

    core::feature_registry gates;
    core::activation_context verified;
    verified.receiver_verified = true;
    verified.material_verified = true;
    verified.resource_ready = true;
    verified.producer_ready = true;
    verified.consumer_verified = true;
    verified.immediate_context = true;
    verified.graph_ready = true;

    CHECK(gates.set(core::operator_id::terminal_sat_rgb, true));
    auto activation = operators::surface::terminal_sat_rgb.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::active);

    // RE-closed terminal RGB SAT: the one-bit modifier is legal only on a
    // verified final separate RGB write; all mismatches fail open.
    std::array<std::uint8_t, 16> shader_bytes{};
    constexpr std::uint32_t terminal_mov_rgb = 0x05000036u;
    constexpr std::uint32_t terminal_mov_sat_rgb = 0x05002036u;
    write_u32(shader_bytes, 4u, terminal_mov_rgb);

    operators::surface::terminal_sat_patch_recipe sat_recipe{
        4u,
        terminal_mov_rgb,
        true
    };

    auto sat_result = operators::surface::apply_terminal_rgb_sat(
        shader_bytes.data(), shader_bytes.size(), sat_recipe);
    CHECK(sat_result == operators::surface::terminal_sat_patch_result::applied);
    CHECK(read_u32(shader_bytes, 4u) == terminal_mov_sat_rgb);

    sat_result = operators::surface::apply_terminal_rgb_sat(
        shader_bytes.data(), shader_bytes.size(), sat_recipe);
    CHECK(sat_result == operators::surface::terminal_sat_patch_result::already_saturated);

    const auto before_mismatch = shader_bytes;
    auto mismatch_recipe = sat_recipe;
    mismatch_recipe.expected_unsaturated_token = 0x01000000u;
    sat_result = operators::surface::apply_terminal_rgb_sat(
        shader_bytes.data(), shader_bytes.size(), mismatch_recipe);
    CHECK(sat_result == operators::surface::terminal_sat_patch_result::fail_open_token_mismatch);
    CHECK(shader_bytes == before_mismatch);

    auto rgba_recipe = sat_recipe;
    rgba_recipe.verified_separate_rgb_write = false;
    sat_result = operators::surface::apply_terminal_rgb_sat(
        shader_bytes.data(), shader_bytes.size(), rgba_recipe);
    CHECK(sat_result == operators::surface::terminal_sat_patch_result::fail_open_unverified_write_shape);
    CHECK(shader_bytes == before_mismatch);

    auto invalid_recipe = sat_recipe;
    invalid_recipe.instruction_byte_offset = 3u;
    sat_result = operators::surface::apply_terminal_rgb_sat(
        shader_bytes.data(), shader_bytes.size(), invalid_recipe);
    CHECK(sat_result == operators::surface::terminal_sat_patch_result::fail_open_invalid_recipe);
    CHECK(shader_bytes == before_mismatch);

    auto out_of_bounds = sat_recipe;
    out_of_bounds.instruction_byte_offset = shader_bytes.size();
    sat_result = operators::surface::apply_terminal_rgb_sat(
        shader_bytes.data(), shader_bytes.size(), out_of_bounds);
    CHECK(sat_result == operators::surface::terminal_sat_patch_result::fail_open_out_of_bounds);
    CHECK(shader_bytes == before_mismatch);

    // Exact PntS attenuation cut: DSR cubes the normalized distance term,
    // while PTDE consumes the same x linearly before saturation.
    CHECK(gates.set(core::operator_id::pointlight_pnts_attenuation, true));
    activation = operators::point_light::pnts_attenuation.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::active);

    auto pnts = operators::point_light::evaluate_pnts_attenuation(5.0f, 0.0f, 10.0f);
    CHECK(pnts.result == operators::point_light::pnts_attenuation_result::exact);
    CHECK(pnts.normalized_x == 0.5f);
    CHECK(pnts.stock_dsr == 0.125f);
    CHECK(pnts.ptde == 0.5f);

    pnts = operators::point_light::evaluate_pnts_attenuation(-10.0f, 0.0f, 10.0f);
    CHECK(pnts.result == operators::point_light::pnts_attenuation_result::exact);
    CHECK(pnts.stock_dsr == 1.0f);
    CHECK(pnts.ptde == 1.0f);

    pnts = operators::point_light::evaluate_pnts_attenuation(15.0f, 0.0f, 10.0f);
    CHECK(pnts.result == operators::point_light::pnts_attenuation_result::exact);
    CHECK(pnts.stock_dsr == 0.0f);
    CHECK(pnts.ptde == 0.0f);

    pnts = operators::point_light::evaluate_pnts_attenuation(5.0f, 10.0f, 10.0f);
    CHECK(pnts.result == operators::point_light::pnts_attenuation_result::fail_open_invalid_range);

    pnts = operators::point_light::evaluate_pnts_attenuation(
        std::numeric_limits<float>::quiet_NaN(), 0.0f, 10.0f);
    CHECK(pnts.result == operators::point_light::pnts_attenuation_result::fail_open_nonfinite_input);

    // Diffuse material-domain island: preserve the DSR pretransform carrier
    // and remove only the certified local abs(x)^2.2 dependency.
    CHECK(gates.set(core::operator_id::diffuse_material_domain, true));
    activation = operators::surface::diffuse_material_domain.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::active);

    auto diffuse_domain =
        operators::surface::evaluate_diffuse_material_domain_host(0.5f);
    CHECK(diffuse_domain.result ==
          operators::surface::diffuse_material_domain_result::exact_local_host_forward);
    CHECK(diffuse_domain.dsr_pretransform == 0.5f);
    CHECK(diffuse_domain.linear_bridge == 0.5f);
    CHECK(std::fabs(diffuse_domain.stock_dsr - 0.21763764f) < 0.000001f);

    diffuse_domain =
        operators::surface::evaluate_diffuse_material_domain_host(1.0f);
    CHECK(diffuse_domain.stock_dsr == 1.0f);
    CHECK(diffuse_domain.linear_bridge == 1.0f);

    diffuse_domain =
        operators::surface::evaluate_diffuse_material_domain_host(
            std::numeric_limits<float>::quiet_NaN());
    CHECK(diffuse_domain.result ==
          operators::surface::diffuse_material_domain_result::fail_open_nonfinite_input);

    // Fixed-family post-Fog island: remove only the DSR conditional root.
    CHECK(gates.set(core::operator_id::fixed_postfog_identity, true));
    activation = operators::surface::fixed_postfog_identity.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::active);

    auto postfog = operators::surface::evaluate_fixed_postfog(0.25f, 1.0f);
    CHECK(postfog.result == operators::surface::fixed_postfog_result::exact);
    CHECK(std::fabs(postfog.stock_dsr - std::pow(0.25f, 1.0f / 2.2f)) < 0.000001f);
    CHECK(postfog.ptde_bridge == 0.25f);

    postfog = operators::surface::evaluate_fixed_postfog(-0.25f, 1.0f);
    CHECK(postfog.result == operators::surface::fixed_postfog_result::exact);
    CHECK(postfog.stock_dsr > 0.0f);
    CHECK(postfog.ptde_bridge == -0.25f);

    postfog = operators::surface::evaluate_fixed_postfog(-0.25f, 0.5f);
    CHECK(postfog.stock_dsr == -0.25f);
    CHECK(postfog.ptde_bridge == -0.25f);

    postfog = operators::surface::evaluate_fixed_postfog(
        0.25f, std::numeric_limits<float>::quiet_NaN());
    CHECK(postfog.result == operators::surface::fixed_postfog_result::fail_open_nonfinite_input);

    // No-Spc EnvSpec deletion is authorized only after the complete
    // exact-homolog + receiver-class + PTDE-absence + alias-safety cut closes.
    CHECK(gates.set(core::operator_id::envspec_nospc_delete, true));
    activation = operators::env_spec::no_spc_delete.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::active);

    operators::env_spec::no_spc_delete_context no_spc_ctx;
    no_spc_ctx.exact_homolog_verified = true;
    no_spc_ctx.substantive_pbl_no_spc = true;
    no_spc_ctx.ptde_envspec_lane_absent = true;
    no_spc_ctx.alias_scope_safe = true;

    auto no_spc = operators::env_spec::evaluate_no_spc_envspec_delete(0.75f, no_spc_ctx);
    CHECK(no_spc.action == operators::env_spec::no_spc_delete_action::delete_dsr_only_envspec);
    CHECK(no_spc.reason == operators::env_spec::no_spc_delete_reason::exact_certified);
    CHECK(no_spc.stock_dsr_envspec_term == 0.75f);
    CHECK(no_spc.bridge_envspec_term == 0.0f);

    auto unsafe_alias = no_spc_ctx;
    unsafe_alias.alias_scope_safe = false;
    no_spc = operators::env_spec::evaluate_no_spc_envspec_delete(0.75f, unsafe_alias);
    CHECK(no_spc.action == operators::env_spec::no_spc_delete_action::preserve_host);
    CHECK(no_spc.reason == operators::env_spec::no_spc_delete_reason::alias_scope_not_safe);
    CHECK(no_spc.bridge_envspec_term == 0.75f);

    auto unverified_pair = no_spc_ctx;
    unverified_pair.exact_homolog_verified = false;
    no_spc = operators::env_spec::evaluate_no_spc_envspec_delete(0.75f, unverified_pair);
    CHECK(no_spc.action == operators::env_spec::no_spc_delete_action::preserve_host);
    CHECK(no_spc.reason == operators::env_spec::no_spc_delete_reason::homolog_not_verified);

    auto ptde_present_or_unknown = no_spc_ctx;
    ptde_present_or_unknown.ptde_envspec_lane_absent = false;
    no_spc = operators::env_spec::evaluate_no_spc_envspec_delete(0.75f, ptde_present_or_unknown);
    CHECK(no_spc.action == operators::env_spec::no_spc_delete_action::preserve_host);
    CHECK(no_spc.reason == operators::env_spec::no_spc_delete_reason::ptde_lane_not_proven_absent);

    // Upper/Lower exact operator math is independent of the still-open
    // runtime producer/sidecar path.
    CHECK(gates.set(core::operator_id::upper_lower, true));
    activation = operators::lightbank::upper_lower.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::active);

    operators::lightbank::ul_raw_endpoint upper_a{{255.0f, 0.0f, 0.0f}, 100.0f};
    operators::lightbank::ul_raw_endpoint lower_a{{0.0f, 255.0f, 0.0f}, 100.0f};
    operators::lightbank::ul_raw_endpoint upper_b{{0.0f, 0.0f, 255.0f}, 100.0f};
    operators::lightbank::ul_raw_endpoint lower_b{{0.0f, 0.0f, 0.0f}, 100.0f};

    auto ul = operators::lightbank::evaluate_upper_lower(
        upper_a, lower_a, upper_b, lower_b, 0.25f, 0.0f);
    CHECK(ul.result == operators::lightbank::upper_lower_result::exact);
    CHECK(std::fabs(ul.upper_ptde.r - 0.75f) < 0.000001f);
    CHECK(std::fabs(ul.upper_ptde.b - 0.25f) < 0.000001f);
    CHECK(std::fabs(ul.lower_ptde.g - 0.75f) < 0.000001f);
    CHECK(ul.hemisphere_t == 0.5f);
    CHECK(std::fabs(ul.hemisphere.r - 0.375f) < 0.000001f);
    CHECK(std::fabs(ul.hemisphere.g - 0.375f) < 0.000001f);
    CHECK(std::fabs(ul.hemisphere.b - 0.125f) < 0.000001f);

    ul = operators::lightbank::evaluate_upper_lower(
        upper_a, lower_a, upper_b, lower_b, 0.25f, 1.0f);
    CHECK(ul.hemisphere_t == 1.0f);
    CHECK(std::fabs(ul.hemisphere.r - ul.upper_ptde.r) < 0.000001f);
    CHECK(std::fabs(ul.hemisphere.b - ul.upper_ptde.b) < 0.000001f);

    ul = operators::lightbank::evaluate_upper_lower(
        upper_a, lower_a, upper_b, lower_b, 0.25f, -1.0f);
    CHECK(ul.hemisphere_t == 0.0f);
    CHECK(std::fabs(ul.hemisphere.g - ul.lower_ptde.g) < 0.000001f);

    auto bad_upper = upper_a;
    bad_upper.multiplier_percent = std::numeric_limits<float>::quiet_NaN();
    ul = operators::lightbank::evaluate_upper_lower(
        bad_upper, lower_a, upper_b, lower_b, 0.25f, 0.0f);
    CHECK(ul.result == operators::lightbank::upper_lower_result::fail_open_nonfinite_input);

    // SpecRGB split-resource bridge requires the complete receiver/material/
    // logical-companion/t10/t1-preservation route. Shared hosts fail open.
    CHECK(gates.set(core::operator_id::spec_rgb, true));
    activation = operators::resource_bridges::spec_rgb.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::active);

    operators::resource_bridges::spec_rgb_context spec_ctx;
    spec_ctx.receiver_id = 33;
    spec_ctx.actual_material_verified = true;
    spec_ctx.material_specular_consumer_verified = true;
    spec_ctx.exact_name_ptde_companion_verified = true;
    spec_ctx.ptde_sidecar_ready = true;
    spec_ctx.native_t10_transport_ready = true;
    spec_ctx.stock_t1_preserved = true;

    auto spec_route = operators::resource_bridges::evaluate_spec_rgb_route(spec_ctx);
    CHECK(spec_route.action == operators::resource_bridges::spec_rgb_action::bind_ptde_t10_rgb);
    CHECK(spec_route.reason == operators::resource_bridges::spec_rgb_reason::active);
    CHECK(spec_route.family == operators::resource_bridges::spec_rgb_receiver_family::dif_spc_bmp);
    CHECK(spec_route.ptde_rgb_srv_slot == 10u);
    CHECK(spec_route.preserve_stock_t1);

    spec_ctx.receiver_id = 40;
    spec_route = operators::resource_bridges::evaluate_spec_rgb_route(spec_ctx);
    CHECK(spec_route.action == operators::resource_bridges::spec_rgb_action::bind_ptde_t10_rgb);
    CHECK(spec_route.family == operators::resource_bridges::spec_rgb_receiver_family::dif_spc);

    spec_ctx.receiver_id = 48;
    spec_route = operators::resource_bridges::evaluate_spec_rgb_route(spec_ctx);
    CHECK(spec_route.action == operators::resource_bridges::spec_rgb_action::preserve_host);
    CHECK(spec_route.reason == operators::resource_bridges::spec_rgb_reason::unsupported_receiver);

    spec_ctx.receiver_id = 33;
    spec_ctx.material_specular_consumer_verified = false;
    spec_route = operators::resource_bridges::evaluate_spec_rgb_route(spec_ctx);
    CHECK(spec_route.action == operators::resource_bridges::spec_rgb_action::preserve_host);
    CHECK(spec_route.reason == operators::resource_bridges::spec_rgb_reason::no_specular_consumer);

    spec_ctx.material_specular_consumer_verified = true;
    spec_ctx.exact_name_ptde_companion_verified = false;
    spec_route = operators::resource_bridges::evaluate_spec_rgb_route(spec_ctx);
    CHECK(spec_route.reason == operators::resource_bridges::spec_rgb_reason::ptde_companion_not_verified);

    spec_ctx.exact_name_ptde_companion_verified = true;
    spec_ctx.stock_t1_preserved = false;
    spec_route = operators::resource_bridges::evaluate_spec_rgb_route(spec_ctx);
    CHECK(spec_route.reason == operators::resource_bridges::spec_rgb_reason::stock_t1_not_preserved);

    // Normal bridge: exact receiver + actual bound t2 identity + exact
    // PTDE sidecar, authorized either by homologous Bmp material route or by a
    // pre-certified application-bound t0+t1+t2 tuple.
    CHECK(gates.set(core::operator_id::normal, true));
    activation = operators::resource_bridges::normal.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::active);

    operators::resource_bridges::normal_bridge_context normal_ctx;
    normal_ctx.receiver_id = 24;
    normal_ctx.authority =
        operators::resource_bridges::normal_route_authority::homologous_material_route;
    normal_ctx.homologous_bmp_material_route = true;
    normal_ctx.target_unambiguous = true;
    normal_ctx.actual_bound_t2_verified = true;
    normal_ctx.exact_ptde_normal_sidecar_verified = true;
    normal_ctx.ptde_srv_ready = true;
    normal_ctx.preserve_stock_s2 = true;

    auto normal_route =
        operators::resource_bridges::evaluate_normal_route(normal_ctx);
    CHECK(normal_route.action ==
          operators::resource_bridges::normal_action::bind_ptde_t2);
    CHECK(normal_route.reason ==
          operators::resource_bridges::normal_reason::active);
    CHECK(normal_route.srv_slot == 2u);
    CHECK(normal_route.sampler_slot == 2u);
    CHECK(normal_route.preserve_stock_sampler);

    normal_ctx.receiver_id = 35;
    normal_route =
        operators::resource_bridges::evaluate_normal_route(normal_ctx);
    CHECK(normal_route.action ==
          operators::resource_bridges::normal_action::bind_ptde_t2);

    normal_ctx.receiver_id = 36;
    normal_route =
        operators::resource_bridges::evaluate_normal_route(normal_ctx);
    CHECK(normal_route.action ==
          operators::resource_bridges::normal_action::preserve_host);
    CHECK(normal_route.reason ==
          operators::resource_bridges::normal_reason::unsupported_receiver);

    normal_ctx.receiver_id = 24;
    normal_ctx.authority =
        operators::resource_bridges::normal_route_authority::safe_exact_resource_tuple;
    normal_ctx.homologous_bmp_material_route = false;
    normal_ctx.safe_exact_t0_t1_t2_tuple = true;
    normal_route =
        operators::resource_bridges::evaluate_normal_route(normal_ctx);
    CHECK(normal_route.action ==
          operators::resource_bridges::normal_action::bind_ptde_t2);

    normal_ctx.safe_exact_t0_t1_t2_tuple = false;
    normal_route =
        operators::resource_bridges::evaluate_normal_route(normal_ctx);
    CHECK(normal_route.action ==
          operators::resource_bridges::normal_action::preserve_host);
    CHECK(normal_route.reason ==
          operators::resource_bridges::normal_reason::route_not_authorized);

    normal_ctx.authority =
        operators::resource_bridges::normal_route_authority::homologous_material_route;
    normal_ctx.homologous_bmp_material_route = false;
    normal_route =
        operators::resource_bridges::evaluate_normal_route(normal_ctx);
    CHECK(normal_route.reason ==
          operators::resource_bridges::normal_reason::nonhomologous_route);

    normal_ctx.homologous_bmp_material_route = true;
    normal_ctx.actual_bound_t2_verified = false;
    normal_route =
        operators::resource_bridges::evaluate_normal_route(normal_ctx);
    CHECK(normal_route.reason ==
          operators::resource_bridges::normal_reason::actual_t2_not_verified);

    normal_ctx.actual_bound_t2_verified = true;
    normal_ctx.target_unambiguous = false;
    normal_route =
        operators::resource_bridges::evaluate_normal_route(normal_ctx);
    CHECK(normal_route.reason ==
          operators::resource_bridges::normal_reason::ambiguous_target);

    normal_ctx.target_unambiguous = true;
    normal_ctx.exact_ptde_normal_sidecar_verified = false;
    normal_route =
        operators::resource_bridges::evaluate_normal_route(normal_ctx);
    CHECK(normal_route.reason ==
          operators::resource_bridges::normal_reason::ptde_sidecar_not_verified);

    normal_ctx.exact_ptde_normal_sidecar_verified = true;
    normal_ctx.preserve_stock_s2 = false;
    normal_route =
        operators::resource_bridges::evaluate_normal_route(normal_ctx);
    CHECK(normal_route.reason ==
          operators::resource_bridges::normal_reason::sampler_contract_not_preserved);

    // Diffuse bridge is an augmentation island. Missing PTDE t0/c100 must
    // preserve an already-valid existing route (e.g. SPEC_ONLY), not disable it.
    CHECK(gates.set(core::operator_id::diffuse, true));
    activation = operators::resource_bridges::diffuse.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::active);

    operators::resource_bridges::diffuse_bridge_context diffuse_ctx;
    diffuse_ctx.receiver_id = 24;
    diffuse_ctx.actual_material_verified = true;
    diffuse_ctx.actual_bound_t0_verified = true;
    diffuse_ctx.exact_ptde_diffuse_companion_verified = true;
    diffuse_ctx.ptde_srv_ready = true;
    diffuse_ctx.ptde_c100_donor_verified = true;
    diffuse_ctx.diffuse_linear_receiver_ready = true;

    auto diffuse_route =
        operators::resource_bridges::evaluate_diffuse_route(diffuse_ctx);
    CHECK(diffuse_route.action ==
          operators::resource_bridges::diffuse_action::bind_ptde_t0_and_full_material_response);
    CHECK(diffuse_route.reason ==
          operators::resource_bridges::diffuse_reason::active);
    CHECK(diffuse_route.srv_slot == 0u);
    CHECK(diffuse_route.use_ptde_c100);
    CHECK(diffuse_route.require_diffuse_linear_receiver);

    diffuse_ctx.receiver_id = 35;
    diffuse_route =
        operators::resource_bridges::evaluate_diffuse_route(diffuse_ctx);
    CHECK(diffuse_route.action ==
          operators::resource_bridges::diffuse_action::bind_ptde_t0_and_full_material_response);

    diffuse_ctx.receiver_id = 36;
    diffuse_route =
        operators::resource_bridges::evaluate_diffuse_route(diffuse_ctx);
    CHECK(diffuse_route.action ==
          operators::resource_bridges::diffuse_action::preserve_existing_route);
    CHECK(diffuse_route.reason ==
          operators::resource_bridges::diffuse_reason::unsupported_receiver);

    diffuse_ctx.receiver_id = 24;
    diffuse_ctx.exact_ptde_diffuse_companion_verified = false;
    diffuse_route =
        operators::resource_bridges::evaluate_diffuse_route(diffuse_ctx);
    CHECK(diffuse_route.action ==
          operators::resource_bridges::diffuse_action::preserve_existing_route);
    CHECK(diffuse_route.reason ==
          operators::resource_bridges::diffuse_reason::ptde_companion_not_verified);

    diffuse_ctx.exact_ptde_diffuse_companion_verified = true;
    diffuse_ctx.ptde_c100_donor_verified = false;
    diffuse_route =
        operators::resource_bridges::evaluate_diffuse_route(diffuse_ctx);
    CHECK(diffuse_route.reason ==
          operators::resource_bridges::diffuse_reason::c100_donor_not_verified);

    diffuse_ctx.ptde_c100_donor_verified = true;
    diffuse_ctx.diffuse_linear_receiver_ready = false;
    diffuse_route =
        operators::resource_bridges::evaluate_diffuse_route(diffuse_ctx);
    CHECK(diffuse_route.reason ==
          operators::resource_bridges::diffuse_reason::diffuse_linear_receiver_not_ready);

    diffuse_ctx.diffuse_linear_receiver_ready = true;
    diffuse_ctx.shared_material_route = true;
    diffuse_ctx.exact_texture_identity_conjunction = false;
    diffuse_route =
        operators::resource_bridges::evaluate_diffuse_route(diffuse_ctx);
    CHECK(diffuse_route.reason ==
          operators::resource_bridges::diffuse_reason::shared_material_texture_identity_missing);

    diffuse_ctx.exact_texture_identity_conjunction = true;
    diffuse_route =
        operators::resource_bridges::evaluate_diffuse_route(diffuse_ctx);
    CHECK(diffuse_route.action ==
          operators::resource_bridges::diffuse_action::bind_ptde_t0_and_full_material_response);

    CHECK(gates.set(core::operator_id::post_hdr, true));
    activation = operators::postprocess::hdr.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::fail_open);
    CHECK(activation.reason == core::activation_reason::blocked);

    CHECK(gates.set(core::operator_id::dsr_native_sfx, true));
    activation = operators::sfx::native_boundary.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::fail_open);
    CHECK(activation.reason == core::activation_reason::stock_host_preserved);

    const auto rejected_rgba =
        core::find_operator_contract(core::operator_id::terminal_sat_rgba);
    CHECK(rejected_rgba.has_value());
    CHECK(rejected_rgba->status == core::canonical_status::rejected);
    CHECK(rejected_rgba->default_state == core::port_state::blocked);

    CHECK(gates.set(core::operator_id::terminal_sat_rgba, true));
    activation = operators::surface::terminal_sat_rgba.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::fail_open);
    CHECK(activation.reason == core::activation_reason::blocked);
    verified.allow_diagnostic = true;
    activation = operators::surface::terminal_sat_rgba.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::fail_open);
    CHECK(activation.reason == core::activation_reason::blocked);
    verified.allow_diagnostic = false;

    auto dec = operators::legacy_plan::decompose_p22_mask(0x27u);
    CHECK(dec.automatic_migration_safe());
    CHECK(dec.unknown_bits == 0u);
    CHECK(dec.nonclosed_bits == 0u);
    CHECK(dec.rejected_bits == 0u);
    CHECK(dec.owner_count == 4u);
    CHECK((dec.closed_bits & 0x01u) != 0u);
    CHECK((dec.closed_bits & 0x02u) != 0u);
    CHECK((dec.closed_bits & 0x04u) != 0u);
    CHECK((dec.closed_bits & 0x20u) != 0u);

    dec = operators::legacy_plan::decompose_p22_mask(0x80000000u);
    CHECK(dec.owner_count == 0u);
    CHECK(dec.unknown_bits == 0x80000000u);
    CHECK(!dec.automatic_migration_safe());

    dec = operators::legacy_plan::decompose_p22_mask(4096u);
    CHECK(dec.rejected_bits == 4096u);
    CHECK(!dec.automatic_migration_safe());

    dec = operators::legacy_plan::decompose_p22_mask(16384u);
    CHECK(dec.rejected_bits == 16384u);
    CHECK(dec.nonclosed_bits == 0u);
    CHECK(dec.owner_count == 1u);
    CHECK(dec.owners[0] == core::operator_id::terminal_sat_rgba);
    CHECK(!dec.automatic_migration_safe());

    dec = operators::legacy_plan::decompose_p22_mask(512u);
    CHECK(dec.nonclosed_bits == 512u);
    CHECK(!dec.automatic_migration_safe());

    CHECK(operators::legacy_plan::legacy_bits_for(core::operator_id::terminal_sat_rgb) ==
          (1u | 8u | 256u | 512u | 2048u | 8192u));
    CHECK(operators::legacy_plan::legacy_bits_for(core::operator_id::diffuse_material_domain) ==
          (2u | 64u | 1024u));

    CHECK(operators::legacy_plan::generated::k_a1_plan_count_v1 == 144u);
    std::size_t a1_aliases = 0;
    std::size_t mask8 = 0;
    std::size_t mask39 = 0;
    std::size_t mask193 = 0;
    std::size_t mask256 = 0;
    for (const auto &plan : operators::legacy_plan::generated::k_a1_plan_index_v1) {
        a1_aliases += plan.alias_count;
        const auto parts = operators::legacy_plan::decompose_p22_mask(plan.legacy_mask);
        CHECK(parts.automatic_migration_safe());
        switch (plan.legacy_mask) {
        case 8u: ++mask8; break;
        case 39u: ++mask39; break;
        case 193u: ++mask193; break;
        case 256u: ++mask256; break;
        default: CHECK(false);
        }
    }
    CHECK(a1_aliases == 252u);
    CHECK(mask8 == 72u);
    CHECK(mask39 == 12u);
    CHECK(mask193 == 24u);
    CHECK(mask256 == 36u);

    CHECK(operators::resource_bridges::spec_rgb.id() == core::operator_id::spec_rgb);
    CHECK(operators::resource_bridges::diffuse.id() == core::operator_id::diffuse);
    CHECK(operators::resource_bridges::normal.id() == core::operator_id::normal);
    // Ps_Body Subsurf final target: exact Subsurf receiver identity routes
    // variant-preservingly to the ABI-compatible ordinary DifSpcBmp receiver,
    // but only after the complete PTDE ordinary surface route and bypass
    // carrier are ready.
    CHECK(gates.set(core::operator_id::subsurface, true));
    activation = operators::resource_bridges::subsurface.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::active);

    operators::resource_bridges::subsurface_route_context subsurf_ctx;
    subsurf_ctx.actual_material_verified = true;
    subsurf_ctx.actual_material_name =
        operators::resource_bridges::k_dsr_body_subsurf_material;
    subsurf_ctx.actual_material_sha256 =
        operators::resource_bridges::k_dsr_body_subsurf_material_sha256;
    subsurf_ctx.actual_receiver_verified = true;
    subsurf_ctx.actual_receiver_name =
        "FRPG_Phn_DifSpcBmp______Csd_HemEnvSubsurf.fpo";
    subsurf_ctx.actual_receiver_sha256 =
        "0b8288d686c8f349ad87352946be51ffd007462f25357326bf47e736e690e511";
    subsurf_ctx.actual_body_texture_verified = true;
    subsurf_ctx.body_spec_texture =
        operators::resource_bridges::subsurface_body_texture::bd_f_body_s;
    subsurf_ctx.stable_hemenv_no_pointlight_draw_verified = true;
    subsurf_ctx.ptde_slot_mapping_verified = true;
    subsurf_ctx.ptde_donor_verified = true;
    subsurf_ctx.ptde_material_name =
        operators::resource_bridges::k_ptde_body_plain_material;
    subsurf_ctx.ptde_material_sha256 =
        operators::resource_bridges::k_ptde_body_plain_material_sha256;
    subsurf_ctx.ptde_plain_surface_target_verified = true;
    subsurf_ctx.target_plain_receiver_ready = true;
    subsurf_ctx.spec_rgb_route_ready = true;
    subsurf_ctx.diffuse_route_ready = true;
    subsurf_ctx.normal_route_ready = true;
    subsurf_ctx.material_response_route_ready = true;
    subsurf_ctx.dsr_subsurf_bypass_carrier_ready = true;

    auto subsurf_route =
        operators::resource_bridges::evaluate_subsurface_route(subsurf_ctx);
    CHECK(subsurf_route.action ==
          operators::resource_bridges::subsurface_route_action::
              route_to_ptde_plain_difspcbmp_surface);
    CHECK(subsurf_route.reason ==
          operators::resource_bridges::subsurface_route_reason::active);
    CHECK(subsurf_route.target_plain_receiver_id == 33u);
    CHECK(subsurf_route.target_plain_receiver_name ==
          "FRPG_Phn_DifSpcBmp______Csd_HemEnv.fpo");
    CHECK(subsurf_route.target_plain_receiver_sha256 ==
          "35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24");
    CHECK(subsurf_route.carrier ==
          operators::resource_bridges::subsurface_bypass_carrier::
              create_time_pixel_shader_substitution);
    CHECK(subsurf_route.bypass_dsr_subsurf);
    CHECK(!subsurf_route.preserve_dsr_sss);

    auto subsurf_no_carrier = subsurf_ctx;
    subsurf_no_carrier.dsr_subsurf_bypass_carrier_ready = false;
    subsurf_route =
        operators::resource_bridges::evaluate_subsurface_route(subsurf_no_carrier);
    CHECK(subsurf_route.action ==
          operators::resource_bridges::subsurface_route_action::preserve_host);
    CHECK(subsurf_route.reason ==
          operators::resource_bridges::subsurface_route_reason::
              dsr_subsurf_bypass_carrier_not_ready);

    auto subsurf_wrong_hash = subsurf_ctx;
    subsurf_wrong_hash.actual_receiver_sha256 =
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    subsurf_route =
        operators::resource_bridges::evaluate_subsurface_route(subsurf_wrong_hash);
    CHECK(subsurf_route.action ==
          operators::resource_bridges::subsurface_route_action::preserve_host);
    CHECK(subsurf_route.reason ==
          operators::resource_bridges::subsurface_route_reason::
              unsupported_receiver_identity);

    const auto subsurface_contract =
        core::find_operator_contract(core::operator_id::subsurface);
    CHECK(subsurface_contract.has_value());
    CHECK(subsurface_contract->status == core::canonical_status::high_confidence);
    CHECK(subsurface_contract->default_state == core::port_state::off);
    CHECK(operators::resource_bridges::subsurface.id() == core::operator_id::subsurface);
    CHECK(operators::lightbank::upper_lower.id() == core::operator_id::upper_lower);
    CHECK(operators::lightbank::hemdir3.id() == core::operator_id::hemdir3);
    CHECK(operators::point_light::pnts_attenuation.id() ==
          core::operator_id::pointlight_pnts_attenuation);

    std::cout << "dsrrl_renderer_island_tests: PASS\n";
    return 0;
}
