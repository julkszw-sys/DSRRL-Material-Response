#include "dsrrl/runtime/material_owner_producer.hpp"
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

    runtime::actual_material_owner_observation exact{};
    exact.flver_sha256=digest(
        "002271e70f2b00efd4d273b3a53b711ece681e6920d22e763af5355498368e20");
    exact.material_slot=0u;
    exact.material_slot_valid=true;
    CHECK(runtime::enrich_exact_owner_mtd_identity(exact));
    CHECK(exact.material.valid);
    CHECK(exact.material.semantic_name_hash==0xce91d872734184bcull);
    CHECK(exact.material.raw_mtd_sha256==digest(
        "14a82945df959cab55bf9475ecba2822248b6a90911c6b017a86e57e2d60a52c"));
    const auto exact_id=runtime::make_actual_material_identity(exact);
    CHECK(exact_id.owner_tuple_exact);
    CHECK(exact_id.material_slot==0u);

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
