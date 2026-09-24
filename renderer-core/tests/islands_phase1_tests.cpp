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

#include <iostream>
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

} // namespace

int main()
{
    material_response_island mr;

    // A globally safe exact receiver may apply Material Response independent
    // of asset category. This is intentionally not equipment-only.
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

    // Shared specular receivers are not globally classified by shader identity.
    // They require exact actual-material routing.
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

    // PTDE has EnvSpec here. Until its island is ready, preserve the DSR host.
    env = operators::env_spec::env_spec_island::gate(d.envspec, false);
    CHECK(env.selected == operators::env_spec::action::preserve_host);
    CHECK(env.ptde_bridge_required);

    env = operators::env_spec::env_spec_island::gate(d.envspec, true);
    CHECK(env.selected == operators::env_spec::action::activate_ptde_bridge);

    // A known hash collision must not be resolved by route/SHA alone.
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

    // Unknown PTDE EnvSpec status never causes deletion.
    env = operators::env_spec::env_spec_island::gate(
        ptde_envspec_presence::unknown,
        false);
    CHECK(env.selected == operators::env_spec::action::preserve_host);

    material_response_island seeded;
    CHECK(register_confirmed_material_routes_v1(seeded) == 35);
    CHECK(seeded.material_profile_count() == 35);

    // Complete operator catalog: every operator_id has exactly one contract.
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

    // Known canonical safety states must remain enforceable independently of
    // user feature toggles.
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

    // A1/P2.2 monolithic mask ownership is decomposed into independent islands.
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

    // Historical Expanded A1 plan identity surface is reproduced exactly at
    // routing/decomposition level: 144 unique bodies / 252 aliases.
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

    // Resource/light/point islands resolve to their independent operator IDs.
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
