#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/material_response/material_response_seed.hpp"
#include "dsrrl/operators/env_spec/env_spec_island.hpp"
#include "dsrrl/operators/resource_bridges/spec_rgb_bridge.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>

using namespace dsrrl;
using namespace dsrrl::operators::material_response;

namespace {

bool check(bool condition,const char *expr,int line)
{
    if(condition) return true;
    std::cerr<<"CHECK FAILED line "<<line<<": "<<expr<<'\n';
    return false;
}
#define CHECK(e) do { if(!check(static_cast<bool>(e),#e,__LINE__)) return 1; } while(false)

int hex_value(char c)
{
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return 10+c-'a';
    if(c>='A'&&c<='F') return 10+c-'A';
    return -1;
}

core::sha256_digest digest(const char *hex)
{
    core::sha256_digest out{};
    for(std::size_t i=0;i<out.size();++i){
        const int hi=hex_value(hex[i*2]);
        const int lo=hex_value(hex[i*2+1]);
        if(hi<0||lo<0) return {};
        out[i]=static_cast<std::uint8_t>((hi<<4)|lo);
    }
    return out;
}

material_identity make_identity(
    std::uint32_t route,const char *name,const char *sha,const char *family)
{
    material_identity out;
    out.valid=true;
    out.route_index=route;
    out.semantic_name_hash=mtd_semantic_hash(name);
    out.raw_mtd_sha256=digest(sha);
    out.material_family_hash=mtd_semantic_hash(family);
    return out;
}

} // namespace

int main()
{
    const auto pmetal=make_identity(
        345u,"P_Metal[DSB].mtd",
        "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b",
        "DifSpcBmp");
    mtd_semantic_query q{pmetal,33u};

    auto d=classify_mtd_semantic(q,mtd_semantic_operator::material_response);
    CHECK(d.state==mtd_semantic_state::use);
    CHECK(d.source==mtd_semantic_source::full24_exact_cohort);
    CHECK(d.exact_identity_match);

    CHECK(classify_mtd_semantic(q,mtd_semantic_operator::spec_rgb).state==
          mtd_semantic_state::use);
    CHECK(classify_mtd_semantic(q,mtd_semantic_operator::hemenv).state==
          mtd_semantic_state::use);
    CHECK(classify_mtd_semantic(q,mtd_semantic_operator::pointlight).state==
          mtd_semantic_state::no_use);

    d=classify_mtd_semantic(q,mtd_semantic_operator::env_spec);
    CHECK(d.state==mtd_semantic_state::use);
    CHECK(d.source==mtd_semantic_source::exact_override);
    CHECK(mtd_envspec_presence(q)==ptde_envspec_presence::present);

    d=classify_mtd_semantic(q,mtd_semantic_operator::diffuse);
    CHECK(d.state==mtd_semantic_state::unknown);
    CHECK(d.exact_identity_match);

    const auto body=make_identity(
        232u,"Ps_Body[DSB].mtd",
        "af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95",
        "DifSpcBmp");
    mtd_semantic_query body_q{body,34u};
    d=classify_mtd_semantic(body_q,mtd_semantic_operator::subsurface);
    CHECK(d.state==mtd_semantic_state::no_use);
    CHECK(d.source==mtd_semantic_source::exact_override);

    const auto leather=make_identity(
        0u,"P_Leather[DSB]_Alp.mtd",
        "4c728a9b5957a75d0eb82b2b77b800829e1973632c7c0690a7e31a074c85e7fb",
        "DifSpcBmp");
    mtd_semantic_query leather_q{leather,35u};
    d=classify_mtd_semantic(leather_q,mtd_semantic_operator::env_spec);
    CHECK(d.state==mtd_semantic_state::unknown);
    CHECK(d.source==mtd_semantic_source::full24_exact_cohort);
    CHECK(d.exact_identity_match);

    auto wrong=q;
    wrong.receiver_id=36u;
    d=classify_mtd_semantic(wrong,mtd_semantic_operator::material_response);
    CHECK(d.state==mtd_semantic_state::unknown);
    CHECK(!d.exact_identity_match);

    auto edge=make_identity(
        5u,"P_Metal[DSB]_Edge.mtd",
        "8b730ce8655401c5cae694ba5535efb088d9d67db2a157d8f142cb5629293485",
        "DifSpcBmp");
    mtd_semantic_query edge_q{edge,33u};
    CHECK(classify_mtd_semantic(
        edge_q,mtd_semantic_operator::material_response).state==
        mtd_semantic_state::use);
    edge.semantic_name_hash=0u;
    edge_q.material=edge;
    CHECK(classify_mtd_semantic(
        edge_q,mtd_semantic_operator::material_response).state==
        mtd_semantic_state::unknown);

    const auto pmetal_spec=make_identity(
        0u,"P_Metal[DSB]_Spec.mtd",
        "c8504bfa64c84d5035bc56ab63e43d319ef69af08c5044e0260b639ab1b78ffc",
        "DifSpcBmp");
    mtd_semantic_query binding_q{pmetal_spec,33u};

    d=classify_mtd_semantic(
        binding_q,mtd_semantic_operator::material_response);
    CHECK(d.state==mtd_semantic_state::use);
    CHECK(d.source==mtd_semantic_source::exact_binding_extension);
    CHECK(d.gate_policy==mtd_gate_policy::direct_exact);
    CHECK(d.exact_identity_match);

    d=classify_mtd_semantic(binding_q,mtd_semantic_operator::pointlight);
    CHECK(d.state==mtd_semantic_state::no_use);

    d=classify_mtd_semantic(binding_q,mtd_semantic_operator::spec_rgb);
    CHECK(d.state==mtd_semantic_state::unknown);
    CHECK(d.exact_identity_match);

    const auto shared_leather=make_identity(
        0u,"P_Leather[DSB].mtd",
        "0f2b9a1012b83c4dfc909c311d29a11147de8c15191bc62398ce0d5e1cc6757e",
        "DifSpcBmp");
    mtd_semantic_query shared_q{shared_leather,35u};
    d=classify_mtd_semantic(
        shared_q,mtd_semantic_operator::material_response);
    CHECK(d.state==mtd_semantic_state::use);
    CHECK(d.source==mtd_semantic_source::exact_binding_extension);
    CHECK(d.gate_policy==mtd_gate_policy::ptde_companion_required);

    auto bad_binding=shared_q;
    bad_binding.material.raw_mtd_sha256[0]^=0xffu;
    d=classify_mtd_semantic(
        bad_binding,mtd_semantic_operator::material_response);
    CHECK(d.state==mtd_semantic_state::unknown);
    CHECK(!d.exact_identity_match);

    operators::resource_bridges::spec_rgb_context spec_ctx;
    spec_ctx.receiver_id=33u;
    spec_ctx.actual_material_verified=true;
    spec_ctx.material_specular_consumer_verified=true;
    spec_ctx.exact_name_ptde_companion_verified=true;
    spec_ctx.ptde_sidecar_ready=true;
    spec_ctx.native_t10_transport_ready=true;
    spec_ctx.stock_t1_preserved=true;

    auto spec_route=
        operators::resource_bridges::evaluate_spec_rgb_route(spec_ctx,q);
    CHECK(spec_route.action==
          operators::resource_bridges::spec_rgb_action::bind_ptde_t10_rgb);

    spec_route=
        operators::resource_bridges::evaluate_spec_rgb_route(spec_ctx,binding_q);
    CHECK(spec_route.action==
          operators::resource_bridges::spec_rgb_action::preserve_host);
    CHECK(spec_route.reason==
          operators::resource_bridges::spec_rgb_reason::
              mtd_census_not_authorized);

    auto env=operators::env_spec::env_spec_island::gate(q,false);
    CHECK(env.selected==operators::env_spec::action::preserve_host);
    CHECK(env.ptde_bridge_required);
    env=operators::env_spec::env_spec_island::gate(q,true);
    CHECK(env.selected==operators::env_spec::action::activate_ptde_bridge);

    env=operators::env_spec::env_spec_island::gate(leather_q,true);
    CHECK(env.selected==operators::env_spec::action::preserve_host);
    CHECK(!env.ptde_bridge_required);

    material_response_island seeded;
    CHECK(register_confirmed_material_routes_v1(seeded)==35u);
    CHECK(seeded.register_receiver_recipe(receiver_recipe{
        33u,
        material_scope_policy::exact_material_required,
        specular_factor_c101,
        ptde_envspec_presence::unknown
    }));
    const auto mr=seeded.evaluate(33u,pmetal);
    CHECK(mr.active);
    CHECK(mr.envspec==ptde_envspec_presence::present);

    std::cout<<"mtd_semantic_census_tests: PASS\n";
    return 0;
}
