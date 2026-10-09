#include "dsrrl/runtime/ptde_metal_envspec_authority.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <cstring>

namespace mr = dsrrl::operators::material_response;
namespace rt = dsrrl::runtime;

namespace {
std::uint8_t digit(char c)
{
    if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c-'0');
    if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c-'a'+10);
    assert(false && "test fixture SHA must be lower hex");
    return 0u;
}

mr::material_identity material(
    const char *name,
    std::uint32_t route,
    const char *sha)
{
    assert(std::strlen(sha) == 64u);
    mr::material_identity identity{};
    identity.valid = true;
    identity.owner_tuple_exact = true;
    identity.material_slot_valid = true;
    identity.material_slot = 2u;
    identity.route_index = route;
    identity.semantic_name_hash = mr::mtd_semantic_hash(name);
    identity.material_family_hash = mr::mtd_semantic_hash("DifSpcBmp");
    identity.flver_sha256[0] = 0xa5u;
    for (std::size_t j = 0u; j < identity.raw_mtd_sha256.size(); ++j)
        identity.raw_mtd_sha256[j] =
            static_cast<std::uint8_t>((digit(sha[j*2u]) << 4u) |
                                      digit(sha[j*2u+1u]));
    return identity;
}

void expect_valid(
    const char *name, std::uint32_t route, const char *sha,
    rt::ptde_metal_envspec_profile expected, std::uint8_t expected_slot = 2u)
{
    auto x = material(name, route, sha);
    const auto *match = rt::match_ptde_metal_envspec_material(x);
    assert(match != nullptr);
    assert(match->route_index == route);
    assert(match->profile == expected);
    assert(match->envspc_slot == expected_slot);
    assert(rt::should_dispatch_ptde_metal_selector_source(x));

    mr::mtd_semantic_query query{};
    query.material = x;
    query.receiver_id = 33u;
    query.ownership.flver_sha256 = x.flver_sha256;
    query.ownership.material_slot = x.material_slot;
    query.ownership.material_slot_valid = x.material_slot_valid;
    query.ownership.exact = x.owner_tuple_exact;
    const auto env = mr::classify_mtd_envspec_semantics(query);
    assert(env.exact_identity_match);
    assert(env.presence == mr::ptde_envspec_presence::present);
    assert(env.router_state == mr::mtd_envspec_router_state::present);
    assert(env.envspc_slot_valid && env.envspc_slot == expected_slot);
    // MTD-based EnvSpec positive routing cannot by itself certify a
    // resource consumer. This fake test FLVER is NOT an asset-ownership
    // proof and must never become a SpecRGB authorization token.


    // All gates must use exact material context, not just a shared shader.
    auto bad = x;
    bad.valid = false;
    assert(rt::match_ptde_metal_envspec_material(bad) == nullptr);
    if (expected != rt::ptde_metal_envspec_profile::pmetal_baseline)
        assert(!rt::should_dispatch_ptde_metal_selector_source(bad));
    bad = x;
    bad.owner_tuple_exact = false;
    assert(rt::match_ptde_metal_envspec_material(bad) == nullptr);
    bad = x;
    bad.material_slot_valid = false;
    assert(rt::match_ptde_metal_envspec_material(bad) == nullptr);
    bad = x;
    bad.route_index += 1u;
    assert(rt::match_ptde_metal_envspec_material(bad) == nullptr);
#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    if (expected != rt::ptde_metal_envspec_profile::pmetal_baseline) {
        bad = x;
        bad.flver_sha256.fill(0u);
        assert(rt::match_ptde_metal_envspec_material(bad) == nullptr);
    }
#endif
    bad = x;
    bad.raw_mtd_sha256[0] ^= 1u;
    assert(rt::match_ptde_metal_envspec_material(bad) == nullptr);
    bad = x;
    bad.semantic_name_hash ^= 1u;
    assert(rt::match_ptde_metal_envspec_material(bad) == nullptr);
}
} // namespace

int main()
{
    using profile = rt::ptde_metal_envspec_profile;
    constexpr auto original_sha =
        "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b";
    expect_valid("P_Metal[DSB].mtd",345u,original_sha,
                 profile::pmetal_baseline);

    constexpr auto alp_sha =
        "45b985a46381199775b0089e9ac6ad5ea14a97b215421bfe29c29bd7ad97535a";
    constexpr auto edge_sha =
        "8b730ce8655401c5cae694ba5535efb088d9d67db2a157d8f142cb5629293485";
    constexpr auto cmetal_sha =
        "ae2e8df867fe2859eef37104c13c939e7e7fe4f703b7fe49c408d0230fc71d85";

#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)
    expect_valid("P_Metal[DSB]_Alp.mtd",2u,alp_sha,profile::pmetal_alp);
    expect_valid("P_Metal[DSB]_Edge.mtd",5u,edge_sha,profile::pmetal_edge);
    expect_valid("C_Metal[DSB].mtd",229u,cmetal_sha,profile::cmetal);
#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    expect_valid("P_RoughCloth[DSB].mtd",6u,
                 "db197a28abd96f55ce54da65dfea74e8788464dd6ae13ca7ceee038c56d281b1",
                 profile::spc_route_6,0u);
    expect_valid("P_Leather[DSB]_Alp.mtd",0u,
                 "4c728a9b5957a75d0eb82b2b77b800829e1973632c7c0690a7e31a074c85e7fb",
                 profile::spc_route_0,1u);
#endif
    auto x = material("P_Metal[DSB]_Alp.mtd",2u,alp_sha);
    assert(rt::is_experimental_ptde_metal_envspec_material(x));
#else
    assert(rt::match_ptde_metal_envspec_material(
        material("P_Metal[DSB]_Alp.mtd",2u,alp_sha)) == nullptr);
    assert(!rt::should_dispatch_ptde_metal_selector_source(
        material("P_Metal[DSB]_Alp.mtd",2u,alp_sha)));
    assert(!rt::should_dispatch_ptde_metal_selector_source(
        material("P_Metal[DSB]_Edge.mtd",5u,edge_sha)));
    assert(!rt::should_dispatch_ptde_metal_selector_source(
        material("C_Metal[DSB].mtd",229u,cmetal_sha)));
    assert(rt::match_ptde_metal_envspec_material(
        material("P_Metal[DSB]_Edge.mtd",5u,edge_sha)) == nullptr);
    assert(rt::match_ptde_metal_envspec_material(
        material("C_Metal[DSB].mtd",229u,cmetal_sha)) == nullptr);
#endif

    // Same SHA and route, DIFFERENT MTD semantic: never inherit Edge.
    assert(rt::match_ptde_metal_envspec_material(
        material("S_Metal[DSB]_Edge.mtd",5u,edge_sha)) == nullptr);
    assert(!rt::should_dispatch_ptde_metal_selector_source(
        material("S_Metal[DSB]_Edge.mtd",5u,edge_sha)));
    // Route345 cannot authorize C_Metal, even if its raw SHA is substituted.
    assert(rt::match_ptde_metal_envspec_material(
        material("C_Metal[DSB].mtd",345u,cmetal_sha)) == nullptr);
    assert(rt::match_ptde_metal_envspec_material(
        material("P_Metal[DSB].mtd",345u,alp_sha)) == nullptr);
    assert(!rt::is_experimental_ptde_metal_envspec_material(
        material("P_Metal[DSB].mtd",345u,original_sha)));
    return 0;
}
