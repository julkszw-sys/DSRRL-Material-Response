#include "dsrrl/runtime/material_owner_producer.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/material_response/material_response_seed.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp"
#include "dsrrl/operators/material_response/generated_exact_binding_mr_v1.hpp"
#include <cstddef>
#include <iostream>
using namespace dsrrl;
namespace generated =
    dsrrl::operators::material_response::generated;

#define CHECK(x) do { if(!(x)){ std::cerr << "CHECK failed: " #x "\n"; return 1; } } while(false)

core::sha256_digest digest(const char *hex)
{
    core::sha256_digest out{};
    auto nibble=[](char ch)->int{
        if(ch>='0'&&ch<='9') return ch-'0';
        if(ch>='a'&&ch<='f') return 10+ch-'a';
        if(ch>='A'&&ch<='F') return 10+ch-'A';
        return -1;
    };
    for(std::size_t i=0;i<out.size();++i){
        const int hi=nibble(hex[i*2]);
        const int lo=nibble(hex[i*2+1]);
        if(hi<0||lo<0) return {};
        out[i]=static_cast<std::uint8_t>((hi<<4)|lo);
    }
    return out;
}

int main()
{
    runtime::actual_material_owner_observation o{};
    o.material.valid = true;
    o.material.semantic_name_hash = 0x1234u;
    o.material_slot = 7u;
    o.material_slot_valid = true;
    auto id = runtime::make_actual_material_identity(o);
    CHECK(!id.owner_tuple_exact);

    o.flver_sha256[0] = 0x42u;
    id = runtime::make_actual_material_identity(o);
    CHECK(id.owner_tuple_exact);
    CHECK(id.material_slot_valid);
    CHECK(id.material_slot == 7u);
    CHECK(id.flver_identity_hash == 0u);
    CHECK(id.flver_sha256[0] == 0x42u);

    // Legacy token is preserved when present, but it is not required for
    // exact producer provenance and cannot substitute for the full digest.
    o.flver_identity_hash = 0x55u;
    id = runtime::make_actual_material_identity(o);
    CHECK(id.owner_tuple_exact);
    CHECK(id.flver_identity_hash == 0x55u);

    o.material.semantic_name_hash = 0u;
    id = runtime::make_actual_material_identity(o);
    CHECK(!id.owner_tuple_exact);

    // This source-complete owner tuple is intentionally outside the current
    // partial generic raw-MTD identity registry. Exact FLVER+slot ownership
    // alone must not manufacture raw-MTD identity from an operator-specific
    // table; unresolved generic identity therefore fails open.
    runtime::actual_material_owner_observation exact{};
    exact.flver_sha256=digest(
        "002271e70f2b00efd4d273b3a53b711ece681e6920d22e763af5355498368e20");
    exact.material_slot=0u;
    exact.material_slot_valid=true;
    CHECK(!runtime::enrich_exact_owner_mtd_identity(exact));
    CHECK(!exact.material.valid);
    const auto exact_id=runtime::make_actual_material_identity(exact);
    CHECK(!exact_id.owner_tuple_exact);

    runtime::actual_material_owner_observation pmetal{};
    pmetal.flver_sha256=digest(
        "008888370225b851bfe3aaa3799a467a208442e7ed298762a0f925f0b205fb39");
    pmetal.material_slot=1u;
    pmetal.material_slot_valid=true;
    CHECK(runtime::enrich_exact_owner_mtd_identity(pmetal));
    CHECK(pmetal.material.valid);
    CHECK(pmetal.material.semantic_name_hash==0xfd72a0409ae13e45ull);
    CHECK(pmetal.material.raw_mtd_sha256==digest(
        "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b"));
    CHECK(pmetal.material.route_index==345u);
    CHECK(pmetal.material.material_family_hash==
          operators::material_response::mtd_semantic_hash("DifSpcBmp"));
    const auto pmetal_id=runtime::make_actual_material_identity(pmetal);
    CHECK(pmetal_id.owner_tuple_exact);
    CHECK(pmetal_id.route_index==345u);

    // Shared-base exact-binding extension: unlike the retail MTD fallback,
    // the authenticated FLVER+slot owner path may carry the
    // PTDE_COMPANION_REQUIRED profile.
    const auto leather_semantic =
        operators::material_response::mtd_semantic_hash(
            "P_Leather[DSB].mtd");
    const generated::exact_binding_mr_record *leather_extension=nullptr;
    for(const auto &record:generated::k_exact_binding_mr_v1)
        if(record.bridge_binding_id==45u)
            leather_extension=&record;
    CHECK(leather_extension!=nullptr);
    CHECK(!leather_extension->runtime_mtd_allowed);

    runtime::actual_material_owner_observation leather{};
    bool leather_owner_found=false;
    for(const auto &group: generated::k_dsr_flver_owner_groups){
        for(std::uint32_t slot=0u;slot<group.material_count;++slot){
            const auto index=
                static_cast<std::size_t>(group.first_material)+slot;
            if(generated::k_dsr_flver_owner_mtd_hashes[index]!=leather_semantic)
                continue;
            leather.flver_sha256=group.flver_sha256;
            leather.material_slot=slot;
            leather.material_slot_valid=true;
            leather_owner_found=true;
            break;
        }
        if(leather_owner_found) break;
    }
    CHECK(leather_owner_found);
    CHECK(runtime::enrich_exact_owner_mtd_identity(leather));
    CHECK(leather.material.valid);
    CHECK(leather.material.semantic_name_hash==leather_semantic);
    CHECK(leather.material.raw_mtd_sha256==
          leather_extension->raw_mtd_sha256);
    CHECK(leather.material.route_index==
          leather_extension->route_tag);
    CHECK(leather.material.material_family_hash==
          operators::material_response::mtd_semantic_hash(
              leather_extension->material_family));
    const auto leather_id=
        runtime::make_actual_material_identity(leather);
    CHECK(leather_id.owner_tuple_exact);
    CHECK(leather_id.route_index==
          leather_extension->route_tag);

    operators::material_response::material_response_island mr{};
    CHECK(operators::material_response::
          register_confirmed_material_receivers_v1(mr)==24u);
    CHECK(operators::material_response::
          register_confirmed_material_routes_v1(mr)==43u);

    const auto shared_without_companion=
        mr.evaluate(33u,leather_id);
    CHECK(!shared_without_companion.active);
    CHECK(shared_without_companion.reason==
          operators::material_response::
              decision_reason::ptde_companion_required);
    CHECK(shared_without_companion.route_index==
          leather_extension->route_tag);

    const auto shared_with_companion=
        mr.evaluate(33u,leather_id,true);
    CHECK(shared_with_companion.active);
    CHECK(shared_with_companion.reason==
          operators::material_response::decision_reason::active);
    CHECK(shared_with_companion.route_index==
          leather_extension->route_tag);

    // Regression lock from the 2026-09-30 owner runtime: M_7Metal[DSB]
    // reached an authenticated FLVER/material owner but direct PointLight
    // rejected it as unknown because the resolver only had generic MR + NoSpc
    // authority. Prove the real owner producer yields the exact raw MTD needed
    // by the dedicated PointLight authority.
    const auto m7_semantic =
        operators::material_response::mtd_semantic_hash(
            "M_7Metal[DSB].mtd");
    runtime::actual_material_owner_observation m7{};
    bool m7_owner_found=false;
    for(const auto &group: generated::k_dsr_flver_owner_groups){
        for(std::uint32_t slot=0u;slot<group.material_count;++slot){
            const auto index=
                static_cast<std::size_t>(group.first_material)+slot;
            if(generated::k_dsr_flver_owner_mtd_hashes[index]!=m7_semantic)
                continue;
            m7.flver_sha256=group.flver_sha256;
            m7.material_slot=slot;
            m7.material_slot_valid=true;
            m7_owner_found=true;
            break;
        }
        if(m7_owner_found) break;
    }
    CHECK(m7_owner_found);
    CHECK(runtime::enrich_exact_owner_mtd_identity(m7));
    CHECK(m7.material.valid);
    CHECK(m7.material.semantic_name_hash==m7_semantic);
    // Generic MR identity and direct PointLight identity deliberately remain
    // separate authorities. The generic owner registry currently resolves
    // this semantic to 184c..., while the exact PointLight table owns 884c...
    // and the PTDE PointLight constants. Do not rewrite generic MR identity.
    CHECK(m7.material.raw_mtd_sha256==digest(
        "184c9c731c98b642d478930a723989e9490c35e642803dd417f4e56f9077c50d"));
    const auto m7_id=
        runtime::make_actual_material_identity(m7);
    CHECK(m7_id.owner_tuple_exact);

    const auto direct_m7=
        mr.evaluate_direct_pointlight_material(
            m7_id,
            true);
    CHECK(direct_m7.active);
    CHECK(direct_m7.reason==
          operators::material_response::decision_reason::active);
    CHECK(direct_m7.c101==2.5f);
    CHECK(direct_m7.ptde_specular_power_verified);
    CHECK(direct_m7.ptde_specular_power==8.5f);

    // Runtime-MTD authority is stricter than the static owner carrier: when
    // actual bytes are observed, the PointLight-local raw SHA must match.
    auto runtime_m7_bad=m7_id;
    runtime_m7_bad.actual_material_exact=true;
    const auto runtime_m7_bad_result=
        mr.evaluate_direct_pointlight_material(
            runtime_m7_bad,
            true);
    CHECK(!runtime_m7_bad_result.active);
    CHECK(runtime_m7_bad_result.reason==
          operators::material_response::decision_reason::unknown_material);

    auto runtime_m7_exact=m7_id;
    runtime_m7_exact.actual_material_exact=true;
    runtime_m7_exact.raw_mtd_sha256=digest(
        "884c9dbff44b174af78b3db22302dd4855fbb5283e8197e430cb998419fe8b8c");
    const auto runtime_m7_exact_result=
        mr.evaluate_direct_pointlight_material(
            runtime_m7_exact,
            true);
    CHECK(runtime_m7_exact_result.active);

    // Direct PointLight NoSpc authority: exact PTDE/DSR homology supplies
    // raw-MTD identity for the owner producer without promoting the material
    // into the ordinary 35-route Material Response cohort.
    const auto nospc_semantic =
        operators::material_response::mtd_semantic_hash(
            "A10_Sky[Dn]_LS.mtd");
    runtime::actual_material_owner_observation nospc{};
    bool nospc_owner_found=false;
    for(const auto &group: generated::k_dsr_flver_owner_groups){
        for(std::uint32_t slot=0u;slot<group.material_count;++slot){
            const auto index=
                static_cast<std::size_t>(group.first_material)+slot;
            if(generated::k_dsr_flver_owner_mtd_hashes[index]!=nospc_semantic)
                continue;
            nospc.flver_sha256=group.flver_sha256;
            nospc.material_slot=slot;
            nospc.material_slot_valid=true;
            nospc_owner_found=true;
            break;
        }
        if(nospc_owner_found) break;
    }
    CHECK(nospc_owner_found);
    CHECK(runtime::enrich_exact_owner_mtd_identity(nospc));
    CHECK(nospc.material.valid);
    CHECK(nospc.material.semantic_name_hash==nospc_semantic);
    CHECK(nospc.material.raw_mtd_sha256==digest(
        "9e15f7569b6e68245e4bcae56cf6204d5c2784cbee132ca7f9da9aa93751690f"));
    CHECK(nospc.material.route_index==0u);
    CHECK(nospc.material.material_family_hash==0u);
    const auto nospc_id=
        runtime::make_actual_material_identity(nospc);
    CHECK(nospc_id.owner_tuple_exact);

    // End-to-end Subsurface authority: locate an exact DSBT owner tuple from
    // the source-complete corpus, then prove owner enrichment reaches the
    // evidence-certified raw-MTD identity without manufacturing a PTDE route.
    const auto dsbt_semantic =
        operators::material_response::mtd_semantic_hash(
            "Ps_Body[DSBT].mtd");
    runtime::actual_material_owner_observation dsbt{};
    bool dsbt_owner_found=false;
    for(const auto &group: generated::k_dsr_flver_owner_groups){
        for(std::uint32_t slot=0u;slot<group.material_count;++slot){
            const auto index=
                static_cast<std::size_t>(group.first_material)+slot;
            if(generated::k_dsr_flver_owner_mtd_hashes[index]!=dsbt_semantic)
                continue;
            dsbt.flver_sha256=group.flver_sha256;
            dsbt.material_slot=slot;
            dsbt.material_slot_valid=true;
            dsbt_owner_found=true;
            break;
        }
        if(dsbt_owner_found) break;
    }
    CHECK(dsbt_owner_found);
    CHECK(runtime::enrich_exact_owner_mtd_identity(dsbt));
    CHECK(dsbt.material.valid);
    CHECK(dsbt.material.semantic_name_hash==dsbt_semantic);
    CHECK(dsbt.material.raw_mtd_sha256==digest(
        "2706080f2b306245b90bf9d3fdfa38624b2ed0452a9b1aff830cf59de69b37d4"));
    CHECK(dsbt.material.route_index==0u);
    const auto dsbt_id=runtime::make_actual_material_identity(dsbt);
    CHECK(dsbt_id.owner_tuple_exact);

    // Wrong slot and unknown FLVER never manufacture a material identity.
    exact.material_slot=0xffffffffu;
    CHECK(!runtime::enrich_exact_owner_mtd_identity(exact));
    CHECK(!exact.material.valid);

    exact.flver_sha256.fill(0xffu);
    exact.material_slot=0u;
    CHECK(!runtime::enrich_exact_owner_mtd_identity(exact));
    CHECK(!exact.material.valid);
    return 0;
}
