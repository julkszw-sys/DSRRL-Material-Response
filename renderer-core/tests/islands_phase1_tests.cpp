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

    CHECK(gates.set(core::operator_id::post_hdr, true));
    activation = operators::postprocess::hdr.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::fail_open);
    CHECK(activation.reason == core::activation_reason::blocked);

    CHECK(gates.set(core::operator_id::dsr_native_sfx, true));
    activation = operators::sfx::native_boundary.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::fail_open);
    CHECK(activation.reason == core::activation_reason::stock_host_preserved);

    CHECK(gates.set(core::operator_id::terminal_sat_rgba, true));
    activation = operators::surface::terminal_sat_rgba.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::fail_open);
    CHECK(activation.reason == core::activation_reason::diagnostic_not_allowed);
    verified.allow_diagnostic = true;
    activation = operators::surface::terminal_sat_rgba.evaluate(gates, verified);
    CHECK(activation.state == core::island_state::active);
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
    CHECK(operators::resource_bridges::subsurface.id() == core::operator_id::subsurface);
    CHECK(operators::lightbank::upper_lower.id() == core::operator_id::upper_lower);
    CHECK(operators::lightbank::hemdir3.id() == core::operator_id::hemdir3);
    CHECK(operators::point_light::pnts_attenuation.id() ==
          core::operator_id::pointlight_pnts_attenuation);

    std::cout << "dsrrl_renderer_island_tests: PASS\n";
    return 0;
}
