// V310 owner/parser exact negative-gate executable contract; NO pixel claims.
#include "dsrrl/runtime/v310_live_parser_owner_sha_join.hpp"
#include <cassert>
#include <cstring>
#include <cstdint>
#include <iostream>

namespace mr=dsrrl::operators::material_response;
namespace v=dsrrl::runtime;

namespace dsrrl::operators::material_response {
std::uint64_t mtd_semantic_hash(const char *s) noexcept {
    std::uint64_t h=14695981039346656037ull;
    for(;*s;++s){h^=static_cast<std::uint8_t>(*s);h*=1099511628211ull;}
    return h;
}
bool has_exact_flver_material_ownership(const mtd_semantic_query &q) noexcept {
    // Test fixture for exact FLVER + semantic + material slot existence.
    return q.ownership.exact && q.ownership.material_slot_valid &&
           q.ownership.material_slot==2u &&
           q.ownership.flver_sha256[0]==0x33u &&
           q.material.semantic_name_hash==
               mtd_semantic_hash(v::v297::k_authored_slot0[0].name);
}
mtd_envspec_semantics classify_mtd_envspec_semantics(
    const mtd_semantic_query &q) noexcept {
    mtd_envspec_semantics e{};
    if(!v::v297::raw_sha_equal(
            q.material.raw_mtd_sha256,
            v::v297::k_authored_slot0[0].dsr_raw_mtd_sha256)) return e;
    e.exact_identity_match=true;
    e.presence=ptde_envspec_presence::present;
    e.router_state=mtd_envspec_router_state::present;
    e.envspc_slot_valid=true;
    e.envspc_slot=0;
    return e;
}
}
int main() {
    const auto &row=v::v297::k_authored_slot0[0];
    mr::material_identity baseline{};
    baseline.valid=true;
    baseline.owner_tuple_exact=true;
    baseline.material_slot_valid=true;
    baseline.material_slot=2u;
    baseline.route_index=0u;
    baseline.semantic_name_hash=mr::mtd_semantic_hash(row.name);
    baseline.flver_sha256[0]=0x33u;
    baseline.raw_mtd_sha256[0]=0x42u; // Stale offline donor.
    v::v298::parser_mtd_entry live{};
    live.material=reinterpret_cast<const void *>(0x1000);
    live.raw_bytes=2540u;
    for(unsigned i=0;i<32u;++i) {
        auto hex=[](char c)->unsigned {return c>='0'&&c<='9'?unsigned(c-'0'):unsigned(c-'a'+10);};
        live.raw[i]=static_cast<unsigned char>(
            (hex(row.dsr_raw_mtd_sha256[2*i])<<4u)|
             hex(row.dsr_raw_mtd_sha256[2*i+1u]));
    }
    // Positive exact source joined to exact model+slot from independent owner.
    auto m=baseline;
    assert(v::v310::reconcile_exact_slot0_owner_parser(m,live));
    assert(m.actual_material_exact && m.raw_mtd_sha256==live.raw);
    assert(m.owner_tuple_exact && m.material_slot_valid &&
           m.flver_sha256==baseline.flver_sha256 &&
           m.material_slot==baseline.material_slot);
    // Negative controls: no owner, wrong model, wrong slot, stale object,
    // wrong parser bytes, wrong material, positive unrelated route.
    m=baseline;m.owner_tuple_exact=false;
    assert(!v::v310::reconcile_exact_slot0_owner_parser(m,live));
    m=baseline;m.flver_sha256[0]=0;
    assert(!v::v310::reconcile_exact_slot0_owner_parser(m,live));
    m=baseline;m.material_slot=3;
    assert(!v::v310::reconcile_exact_slot0_owner_parser(m,live));
    m=baseline;m.material_slot_valid=false;
    assert(!v::v310::reconcile_exact_slot0_owner_parser(m,live));
    m=baseline;m.semantic_name_hash=mr::mtd_semantic_hash("P_Metal[DSB].mtd");
    assert(!v::v310::reconcile_exact_slot0_owner_parser(m,live));
    m=baseline;m.route_index=345;
    assert(!v::v310::reconcile_exact_slot0_owner_parser(m,live));
    m=baseline;m.actual_material_exact=true;
    assert(!v::v310::reconcile_exact_slot0_owner_parser(m,live));
    m=baseline;m.raw_mtd_sha256=live.raw;
    assert(!v::v310::reconcile_exact_slot0_owner_parser(m,live));
    m=baseline;auto invalid=live;invalid.raw[0]^=0xff;
    assert(!v::v310::reconcile_exact_slot0_owner_parser(m,invalid));
    m=baseline;invalid=live;invalid.raw_bytes=0;
    assert(!v::v310::reconcile_exact_slot0_owner_parser(m,invalid));
    m=baseline;invalid=live;invalid.material=nullptr;
    assert(!v::v310::reconcile_exact_slot0_owner_parser(m,invalid));
    std::cout<<"V310_EXACT_ORIGINAL_FLVER_SLOT_PARSER_SHA_NEGATIVE_CONTRACT_PASS\n";
}
