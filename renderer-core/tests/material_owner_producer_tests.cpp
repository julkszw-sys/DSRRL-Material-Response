#include "dsrrl/runtime/material_owner_producer.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuple_v1.hpp"
#include <cstddef>
#include <iostream>
using namespace dsrrl;

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

    // End-to-end Subsurface authority: locate an exact DSBT owner tuple from
    // the source-complete corpus, then prove owner enrichment reaches the
    // evidence-certified raw-MTD identity without manufacturing a PTDE route.
    namespace generated =
        operators::material_response::generated;
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
