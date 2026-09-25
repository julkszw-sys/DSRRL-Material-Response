#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/material_response/material_response_seed.hpp"
#include "dsrrl/operators/material_response/generated_envspec_router_v1.hpp"
#include "dsrrl/operators/material_response/generated_ptde_flver_texture_semantics_v1.hpp"
#include "dsrrl/operators/material_response/generated_flver_pairwise_semantics_v1.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp"
#include "dsrrl/operators/material_response/generated_mtd_spx_negative_v1.hpp"
#include "dsrrl/operators/env_spec/env_spec_island.hpp"
#include "dsrrl/operators/resource_bridges/spec_rgb_bridge.hpp"

#include <array>
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
    CHECK(d.source==mtd_semantic_source::envspec_router_exact);
    CHECK(mtd_envspec_presence(q)==ptde_envspec_presence::present);
    const auto pmetal_env=classify_mtd_envspec_semantics(q);
    CHECK(pmetal_env.exact_identity_match);
    CHECK(pmetal_env.router_state==mtd_envspec_router_state::present);
    CHECK(pmetal_env.envspc_slot_valid);
    CHECK(pmetal_env.envspc_slot==2u);
    CHECK(!pmetal_env.suppress_dsr_only_safe);

    const auto pmetal_legacy=classify_mtd_envspec_semantics_legacy(
        0x1755dba68cb5e9a3ull,
        pmetal.raw_mtd_sha256);
    CHECK(pmetal_legacy.exact_identity_match);
    CHECK(pmetal_legacy.router_state==mtd_envspec_router_state::present);
    CHECK(pmetal_legacy.envspc_slot_valid);
    CHECK(pmetal_legacy.envspc_slot==2u);

    // Runtime may provide a nonzero parser-side semantic discriminator that
    // differs from the canonical basename hash. The raw SHA fallback is legal
    // only because the generated router proves SHA -> EnvSpec semantics is
    // unambiguous.
    const auto wrong_legacy=classify_mtd_envspec_semantics_legacy(
        0x1755dba68cb5e9a2ull,
        pmetal.raw_mtd_sha256);
    CHECK(wrong_legacy.exact_identity_match);
    CHECK(wrong_legacy.router_state==mtd_envspec_router_state::present);
    CHECK(wrong_legacy.envspc_slot_valid);
    CHECK(wrong_legacy.envspc_slot==2u);

    const auto missing_key_legacy=classify_mtd_envspec_semantics_legacy(
        0u,
        pmetal.raw_mtd_sha256);
    CHECK(missing_key_legacy.exact_identity_match);
    CHECK(missing_key_legacy.router_state==mtd_envspec_router_state::present);
    CHECK(missing_key_legacy.envspc_slot_valid);
    CHECK(missing_key_legacy.envspc_slot==2u);

    auto unknown_sha=pmetal.raw_mtd_sha256;
    unknown_sha[0]^=0xffu;
    const auto unknown_legacy=classify_mtd_envspec_semantics_legacy(
        0x1755dba68cb5e9a2ull,
        unknown_sha);
    CHECK(!unknown_legacy.exact_identity_match);

    // Owner FLVER census proves positive PTDE texture capability for this
    // exact material. Exact DSR host identity is independently joined through
    // the homology-derived 325-record router. No per-draw FLVER guess is needed
    // for positive USE; resource tuple checks remain downstream in runtime.
    const auto pmetal_tex=classify_ptde_flver_texture_semantics(pmetal);
    CHECK(pmetal_tex.exact_host_identity_match);
    CHECK(pmetal_tex.source_complete);
    CHECK(ptde_flver_texture_semantic_present(
        pmetal_tex,ptde_texture_semantic::diffuse));
    CHECK(ptde_flver_texture_semantic_present(
        pmetal_tex,ptde_texture_semantic::bump));
    CHECK(ptde_flver_texture_semantic_present(
        pmetal_tex,ptde_texture_semantic::specular));

    const auto pmetal_tex_legacy=
        classify_ptde_flver_texture_semantics_legacy(
            0x1755dba68cb5e9a3ull,
            pmetal.raw_mtd_sha256);
    CHECK(pmetal_tex_legacy.exact_host_identity_match);
    CHECK(pmetal_tex_legacy.positive_mask==pmetal_tex.positive_mask);

    // Legacy parser keys are not always the canonical basename hash. Raw DSR
    // SHA fallback is allowed only when every exact host alias for that SHA has
    // a PTDE capability row and all aliases agree on the positive mask.
    const auto pmetal_tex_fallback=
        classify_ptde_flver_texture_semantics_legacy(
            0x1755dba68cb5e9a2ull,
            pmetal.raw_mtd_sha256);
    CHECK(pmetal_tex_fallback.exact_host_identity_match);
    CHECK(pmetal_tex_fallback.positive_mask==pmetal_tex.positive_mask);

    // Generic semantic API remains ownership-sensitive. Runtime may use the
    // direct positive capability classifier only at its independently exact
    // material-pointer + certified receiver + resource-tuple cut.
    d=classify_mtd_semantic(q,mtd_semantic_operator::diffuse);
    CHECK(d.state==mtd_semantic_state::unknown);
    CHECK(!d.exact_identity_match);

    d=classify_mtd_semantic(q,mtd_semantic_operator::normal_bump);
    CHECK(d.state==mtd_semantic_state::unknown);
    CHECK(!d.exact_identity_match);

    // A caller cannot self-certify FLVER ownership. The current pairwise
    // corpus is MTD-aggregate evidence and does not yet materialize exact
    // (DSR FLVER identity, material slot, MTD) tuples. Even a fully populated
    // ownership struct must therefore fail open until that tuple corpus exists.
    CHECK(!generated::k_dsr_flver_owner_tuple_source_complete);
    auto owned_q=q;
    owned_q.ownership.flver_sha256[0]=0x12u;
    owned_q.ownership.flver_identity_hash=0x1234u; // legacy auxiliary token only
    owned_q.ownership.material_slot=7u;
    owned_q.ownership.material_slot_valid=true;
    owned_q.ownership.exact=true;
    CHECK(!has_exact_flver_material_ownership(owned_q));

    d=classify_mtd_semantic(owned_q,mtd_semantic_operator::diffuse);
    CHECK(d.state==mtd_semantic_state::unknown);
    CHECK(d.source==mtd_semantic_source::none);
    CHECK(!d.exact_identity_match);

    d=classify_mtd_semantic(owned_q,mtd_semantic_operator::normal_bump);
    CHECK(d.state==mtd_semantic_state::unknown);
    CHECK(d.source==mtd_semantic_source::none);
    CHECK(!d.exact_identity_match);

    auto spoofed_owner=owned_q;
    spoofed_owner.ownership.flver_sha256.fill(0xffu);
    spoofed_owner.ownership.flver_identity_hash=0xffffffffffffffffull;
    spoofed_owner.ownership.material_slot=0xffffffffu;
    CHECK(!has_exact_flver_material_ownership(spoofed_owner));
    CHECK(classify_mtd_semantic(
        spoofed_owner,mtd_semantic_operator::diffuse).state==
        mtd_semantic_state::unknown);

    auto wrong_tex=pmetal;
    wrong_tex.raw_mtd_sha256[0]^=0xffu;
    const auto wrong_tex_sem=
        classify_ptde_flver_texture_semantics(wrong_tex);
    CHECK(!wrong_tex_sem.exact_host_identity_match);

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
    CHECK(d.state==mtd_semantic_state::use);
    CHECK(d.source==mtd_semantic_source::envspec_router_exact);
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
    const auto edge_tex_exact=
        classify_ptde_flver_texture_semantics(edge);
    CHECK(edge_tex_exact.exact_host_identity_match);
    CHECK(ptde_flver_texture_semantic_present(
        edge_tex_exact,ptde_texture_semantic::diffuse));
    const auto edge_tex_ambiguous_fallback=
        classify_ptde_flver_texture_semantics_legacy(
            0x075f111448f91c2eull,
            edge.raw_mtd_sha256);
    CHECK(!edge_tex_ambiguous_fallback.exact_host_identity_match);
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
    CHECK(env.selected==operators::env_spec::action::activate_ptde_bridge);
    CHECK(env.ptde_bridge_required);

    // The exact 325-record router separates PTDE semantic absence from the
    // narrower carrier-safe explicit-none cohort.
    const auto mdb=make_identity(
        0u,"M[DB].mtd",
        "afe70656efd6c7797fccce5a43e7c4738eb3fec5177dd2c787f68438332d238f",
        "DifSpcBmp");
    mtd_semantic_query mdb_q{mdb,33u};
    d=classify_mtd_semantic(mdb_q,mtd_semantic_operator::env_spec);
    CHECK(d.state==mtd_semantic_state::no_use);
    CHECK(d.source==mtd_semantic_source::envspec_router_exact);
    const auto mdb_env=classify_mtd_envspec_semantics(mdb_q);
    CHECK(mdb_env.router_state==mtd_envspec_router_state::explicit_none);
    CHECK(mdb_env.suppress_dsr_only_safe);
    CHECK(mdb_env.envspc_slot_valid);
    CHECK(mdb_env.envspc_slot==0u);
    env=operators::env_spec::env_spec_island::gate(mdb_q,false);
    CHECK(env.selected==operators::env_spec::action::suppress_dsr_only);

    const auto unsafe_none=make_identity(
        0u,"A01[D]_Alp.mtd",
        "89076b688246989ed9d7c65da4df4508fd2c1ec13c8f929fd1ec852507f7e4ad",
        "DifSpcBmp");
    mtd_semantic_query unsafe_none_q{unsafe_none,33u};
    d=classify_mtd_semantic(
        unsafe_none_q,mtd_semantic_operator::env_spec);
    CHECK(d.state==mtd_semantic_state::no_use);
    const auto unsafe_env=
        classify_mtd_envspec_semantics(unsafe_none_q);
    CHECK(unsafe_env.router_state==
          mtd_envspec_router_state::explicit_none);
    CHECK(!unsafe_env.suppress_dsr_only_safe);
    env=operators::env_spec::env_spec_island::gate(
        unsafe_none_q,false);
    CHECK(env.selected==operators::env_spec::action::preserve_host);

    const auto nospc=make_identity(
        0u,"A10_BG_cloud[Dn]_Add.mtd",
        "b3ec0ccf59bfdac94db7eb6292484c43755c09252a711b4aa2f2131b32c33543",
        "DifSpcBmp");
    mtd_semantic_query nospc_q{nospc,33u};
    d=classify_mtd_semantic(nospc_q,mtd_semantic_operator::env_spec);
    CHECK(d.state==mtd_semantic_state::no_use);
    const auto nospc_env=classify_mtd_envspec_semantics(nospc_q);
    CHECK(nospc_env.router_state==mtd_envspec_router_state::nospc_host);
    CHECK(!nospc_env.suppress_dsr_only_safe);
    env=operators::env_spec::env_spec_island::gate(nospc_q,false);
    CHECK(env.selected==operators::env_spec::action::preserve_host);

    // Source-level census integrity: 325 exact material identities,
    // 213 PRESENT, 87 EXPLICIT_NONE, 25 NOSPC_HOST and exactly 20
    // carrier-safe explicit-none rows.
    CHECK(generated::k_envspec_router_v1.size()==325u);
    std::size_t present_count=0u;
    std::size_t explicit_none_count=0u;
    std::size_t nospc_count=0u;
    std::size_t safe_none_count=0u;
    std::array<std::size_t,4> slot_counts{};
    for(const auto &record:generated::k_envspec_router_v1){
        CHECK(record.envspc_slot<slot_counts.size());
        ++slot_counts[record.envspc_slot];
        if(record.explicit_none_safe)
            ++safe_none_count;
        switch(record.state){
        case generated::envspec_router_state::present:
            ++present_count;
            break;
        case generated::envspec_router_state::explicit_none:
            ++explicit_none_count;
            break;
        case generated::envspec_router_state::nospc_host:
            ++nospc_count;
            break;
        case generated::envspec_router_state::unknown:
        default:
            CHECK(false);
        }
    }
    CHECK(present_count==213u);
    CHECK(explicit_none_count==87u);
    CHECK(nospc_count==25u);
    CHECK(safe_none_count==20u);
    CHECK(slot_counts[0]==179u);
    CHECK(slot_counts[1]==48u);
    CHECK(slot_counts[2]==54u);
    CHECK(slot_counts[3]==44u);


    // FIXED owner FLVER census closed the variable-header BND3 parser gap:
    // zero scan errors / zero unresolved rows. Absence is still not promoted
    // blindly; Diffuse/Normal additionally require the DSR<->PTDE pairwise
    // shared-stable resource gate.
    CHECK(generated::k_ptde_flver_texture_semantics_v1.size()==261u);
    CHECK(generated::k_ptde_flver_texture_semantics_scan_error_count==0u);
    CHECK(generated::k_ptde_flver_texture_semantics_source_complete);
    CHECK(generated::k_flver_pairwise_overlap_mtd_count==256u);
    CHECK(generated::k_flver_pairwise_diffuse_reject.size()==6u);
    CHECK(generated::k_flver_pairwise_bump_reject.size()==6u);
    CHECK(generated::flver_pairwise_diffuse_stable_after_ptde_positive(
        mtd_semantic_hash("P_Metal[DSB].mtd")));
    CHECK(generated::flver_pairwise_bump_stable_after_ptde_positive(
        mtd_semantic_hash("P_Metal[DSB].mtd")));
    CHECK(!generated::flver_pairwise_diffuse_stable_after_ptde_positive(
        mtd_semantic_hash("P[D].mtd")));
    CHECK(!generated::flver_pairwise_bump_stable_after_ptde_positive(
        mtd_semantic_hash("Ps_Wander_Ghost.mtd")));

    std::size_t ptde_diffuse_count=0u;
    std::size_t ptde_bump_count=0u;
    std::size_t ptde_spec_count=0u;
    std::size_t exact_host_overlap=0u;
    std::size_t exact_host_diffuse=0u;
    std::size_t exact_host_bump=0u;
    std::size_t exact_host_spec=0u;
    for(std::size_t i=0;i<generated::k_ptde_flver_texture_semantics_v1.size();++i){
        const auto &cap=generated::k_ptde_flver_texture_semantics_v1[i];
        CHECK(cap.positive_mask!=0u);
        if(cap.positive_mask&generated::ptde_tex_diffuse) ++ptde_diffuse_count;
        if(cap.positive_mask&generated::ptde_tex_bump) ++ptde_bump_count;
        if(cap.positive_mask&generated::ptde_tex_specular) ++ptde_spec_count;
        for(std::size_t j=i+1u;j<generated::k_ptde_flver_texture_semantics_v1.size();++j){
            const auto &other=generated::k_ptde_flver_texture_semantics_v1[j];
            CHECK(cap.semantic_name_hash!=other.semantic_name_hash);
            CHECK(cap.legacy_name_hash_utf16_lower!=other.legacy_name_hash_utf16_lower);
        }
        bool exact_host=false;
        for(const auto &host:generated::k_envspec_router_v1){
            if(host.legacy_name_hash_utf16_lower==
               cap.legacy_name_hash_utf16_lower){
                exact_host=true;
                break;
            }
        }
        if(exact_host){
            ++exact_host_overlap;
            if(cap.positive_mask&generated::ptde_tex_diffuse) ++exact_host_diffuse;
            if(cap.positive_mask&generated::ptde_tex_bump) ++exact_host_bump;
            if(cap.positive_mask&generated::ptde_tex_specular) ++exact_host_spec;
        }
    }
    CHECK(ptde_diffuse_count==239u);
    CHECK(ptde_bump_count==179u);
    CHECK(ptde_spec_count==153u);
    CHECK(exact_host_overlap==204u);
    CHECK(exact_host_diffuse==204u);
    CHECK(exact_host_bump==141u);
    CHECK(exact_host_spec==147u);

    // Raw PTDE<->DSR MTD/SPX census: exact negative routing gate. DSR may
    // expose Spc/Bmp lanes that did not exist in the corresponding PTDE SPX
    // contract. Such DSR-only lanes must not positively authorize SpecRGB or
    // Normal/Bump islands.
    CHECK(generated::k_mtd_spx_negative_record_count==118u);
    std::size_t spx_added_specular=0u;
    std::size_t spx_added_bump=0u;
    for(const auto &record:generated::k_mtd_spx_negative_v1){
        if(record.dsr_only_added_mask&generated::mtd_spx_feature_specular)
            ++spx_added_specular;
        if(record.dsr_only_added_mask&generated::mtd_spx_feature_bump)
            ++spx_added_bump;
    }
    CHECK(spx_added_specular==87u);
    CHECK(spx_added_bump==98u);

    const auto a01_added_spcbmp=make_identity(
        0u,"A01[D]_Alp.mtd",
        "89076b688246989ed9d7c65da4df4508fd2c1ec13c8f929fd1ec852507f7e4ad",
        "DifSpcBmp");
    mtd_semantic_query a01_spx_q{a01_added_spcbmp,33u};
    d=classify_mtd_semantic(a01_spx_q,mtd_semantic_operator::spec_rgb);
    CHECK(d.state==mtd_semantic_state::no_use);
    CHECK(d.source==mtd_semantic_source::mtd_spx_pairwise_negative_exact);
    CHECK(d.exact_identity_match);

    // Exact negative MTD/SPX evidence is conservative and does not need an
    // unauthenticated positive owner claim. It may still suppress a DSR-only
    // Bump lane while positive Normal routing remains held open.
    d=classify_mtd_semantic(a01_spx_q,mtd_semantic_operator::normal_bump);
    CHECK(d.state==mtd_semantic_state::no_use);
    CHECK(d.source==mtd_semantic_source::mtd_spx_pairwise_negative_exact);
    CHECK(d.exact_identity_match);

    // Raw MTD SHA is a safe EnvSpec-semantic fallback only while every
    // duplicate payload in the canonical router agrees on state, slot and
    // explicit-none safety. This invariant makes future generator drift
    // fail CI instead of broadening runtime routing silently.
    for(std::size_t i=0;i<generated::k_envspec_router_v1.size();++i){
        for(std::size_t j=i+1u;j<generated::k_envspec_router_v1.size();++j){
            const auto &a=generated::k_envspec_router_v1[i];
            const auto &b=generated::k_envspec_router_v1[j];
            if(a.raw_mtd_sha256!=b.raw_mtd_sha256)
                continue;
            CHECK(a.state==b.state);
            CHECK(a.envspc_slot==b.envspc_slot);
            CHECK(a.explicit_none_safe==b.explicit_none_safe);
        }
    }

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