#include "dsrrl/operators/env_spec/env_spec_island.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/material_response_seed.hpp"

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

    std::cout << "dsrrl_renderer_island_tests: PASS\n";
    return 0;
}
