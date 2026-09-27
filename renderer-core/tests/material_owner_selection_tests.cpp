#include "dsrrl/runtime/material_owner_selection.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"

#include <cstddef>
#include <cstdint>
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
    namespace mr = operators::material_response;

    runtime::material_owner_selection_reset_stats();

    mr::material_identity known{};
    known.valid=true;
    known.owner_tuple_exact=true;
    known.flver_sha256=digest(
        "002271e70f2b00efd4d273b3a53b711ece681e6920d22e763af5355498368e20");
    known.material_slot=0u;
    known.material_slot_valid=true;
    known.semantic_name_hash=0xce91d872734184bcull;

    CHECK(runtime::material_owner_selection_publish(known));

    mr::material_identity observed{};
    CHECK(runtime::material_owner_selection_consume(observed));
    CHECK(observed.owner_tuple_exact);
    CHECK(observed.flver_sha256==known.flver_sha256);
    CHECK(observed.material_slot==0u);
    CHECK(observed.semantic_name_hash==known.semantic_name_hash);

    // Consume-once: the same selector identity cannot leak to a later draw.
    CHECK(!runtime::material_owner_selection_consume(observed));

    CHECK(runtime::material_owner_selection_publish(known));
    runtime::material_owner_selection_clear();
    CHECK(!runtime::material_owner_selection_consume(observed));

    auto spoofed=known;
    spoofed.flver_sha256.fill(0xffu);
    CHECK(!runtime::material_owner_selection_publish(spoofed));
    CHECK(!runtime::material_owner_selection_consume(observed));

    // Legacy-compatible P_Metal authority is intentionally independent from
    // FLVER ownership, but only for the exact runtime material identity.
    mr::material_identity runtime_pmetal{};
    runtime_pmetal.valid=true;
    runtime_pmetal.actual_material_exact=true;
    runtime_pmetal.route_index=345u;
    runtime_pmetal.semantic_name_hash=
        mr::mtd_semantic_hash("P_Metal[DSB].mtd");
    runtime_pmetal.raw_mtd_sha256=digest(
        "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b");
    runtime_pmetal.material_family_hash=
        mr::mtd_semantic_hash("DifSpcBmp");

    CHECK(runtime::material_owner_selection_publish(runtime_pmetal));
    CHECK(runtime::material_owner_selection_consume(observed));
    CHECK(observed.actual_material_exact);
    CHECK(!observed.owner_tuple_exact);
    CHECK(observed.route_index==345u);

    auto wrong_runtime=runtime_pmetal;
    wrong_runtime.route_index=359u;
    CHECK(!runtime::material_owner_selection_publish(wrong_runtime));
    CHECK(!runtime::material_owner_selection_consume(observed));

    const auto stats=runtime::material_owner_selection_stats();
    CHECK(stats.selector_events==5u);
    CHECK(stats.accepted_callers==3u);
    CHECK(stats.owner_enriched==4u);
    CHECK(stats.owner_authenticated==2u);
    CHECK(stats.actual_material_authenticated==1u);
    CHECK(stats.fail_open==2u);

    runtime::material_owner_selection_reset_stats();
    return 0;
}
