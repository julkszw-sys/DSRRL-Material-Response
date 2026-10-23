// V311 exact live FLVER parser + exact live retail MTD source:
// independent model+slot+material+SHA tests; rejects missing source & C_Metal.
#include "dsrrl/runtime/v311_live_direct_owner_recovery.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
namespace mr=dsrrl::operators::material_response;
namespace v=dsrrl::runtime;
namespace {
std::uint64_t expected_semantic=0;
std::uint8_t expected_env_slot=0u;
bool model_lookup(const dsrrl::core::sha256_digest &flver,
                  std::uint32_t slot,
                  std::uint64_t &semantic) noexcept {
    semantic=0;
    if(flver[0]!=0x55u||slot!=7u)return false;
    semantic=expected_semantic;return semantic!=0;
}
void set_raw(dsrrl::core::sha256_digest &raw,const char *hex) {
    for(unsigned i=0;i<32u;++i) {
        auto nib=[](char x)->unsigned {return x>='0'&&x<='9'?unsigned(x-'0'):unsigned(x-'a'+10);};
        raw[i]=static_cast<std::uint8_t>((nib(hex[2*i])<<4)|nib(hex[2*i+1u]));
    }
}
}
namespace dsrrl::operators::material_response {
std::uint64_t mtd_semantic_hash(const char *s) noexcept {
    std::uint64_t h=14695981039346656037ull;
    for(;*s;++s){h^=static_cast<std::uint8_t>(*s);h*=1099511628211ull;}
    return h;
}
bool has_exact_flver_material_ownership(const mtd_semantic_query &q) noexcept {
    return q.ownership.exact &&
        q.ownership.flver_sha256[0]==0x55u &&
        q.ownership.material_slot_valid && q.ownership.material_slot==7u &&
        q.material.semantic_name_hash==expected_semantic &&
        q.material.raw_mtd_sha256[0]!=0u;
}
mtd_envspec_semantics classify_mtd_envspec_semantics(const mtd_semantic_query &q) noexcept {
    mtd_envspec_semantics x{};
    if(q.material.semantic_name_hash!=expected_semantic)return x;
    x.exact_identity_match=true;
    x.presence=ptde_envspec_presence::present;
    x.router_state=mtd_envspec_router_state::present;
    x.envspc_slot_valid=true;
    x.envspc_slot=expected_env_slot;
    return x;
}
}
int main() {
    dsrrl::core::sha256_digest model{};model[0]=0x55u;
    v::v298::parser_mtd_entry live{};
    live.material=reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1000));
    live.raw_bytes=2540u;
    mr::material_identity got{};
    const auto &rough=v::v297::k_authored_slot0[0];
    expected_semantic=mr::mtd_semantic_hash(rough.name);
    expected_env_slot=0u;
    set_raw(live.raw,rough.dsr_raw_mtd_sha256);
    assert(v::v311::recover_exact_owner_from_live_parsers(
        model,7,live.material,live,&model_lookup,got));
    assert(got.valid && got.owner_tuple_exact &&
           got.actual_material_exact && got.flver_sha256==model &&
           got.raw_mtd_sha256==live.raw && got.material_slot==7u &&
           got.route_index==0u);
    mr::material_identity rejected{};
    auto altered=live;altered.raw[0]^=0xffu;
    assert(!v::v311::recover_exact_owner_from_live_parsers(
        model,7,live.material,altered,&model_lookup,rejected));
    altered=live;altered.material=reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x2000));
    assert(!v::v311::recover_exact_owner_from_live_parsers(
        model,7,live.material,altered,&model_lookup,rejected));
    altered=live;altered.raw_bytes=0;
    assert(!v::v311::recover_exact_owner_from_live_parsers(
        model,7,live.material,altered,&model_lookup,rejected));
    assert(!v::v311::recover_exact_owner_from_live_parsers(
        model,6,live.material,live,&model_lookup,rejected));
    auto missing=model;missing[0]=0x66u;
    assert(!v::v311::recover_exact_owner_from_live_parsers(
        missing,7,live.material,live,&model_lookup,rejected));
    assert(!v::v311::recover_exact_owner_from_live_parsers(
        model,7,live.material,live,nullptr,rejected));
    // A nonhomologous C_Metal cannot be authorized by its texture name:
    expected_semantic=mr::mtd_semantic_hash("C_Metal[DSB].mtd");
    assert(!v::v311::recover_exact_owner_from_live_parsers(
        model,7,live.material,live,&model_lookup,rejected));
    // Slot1 Leather belongs to an INDEPENDENT original EnvSpec family.
    const auto &leather=v::v300::k_leather[1];
    expected_semantic=mr::mtd_semantic_hash(leather.name);
    expected_env_slot=1u;
    set_raw(live.raw,leather.sha);
    assert(v::v311::recover_exact_owner_from_live_parsers(
        model,7,live.material,live,&model_lookup,got));
    expected_env_slot=0u;
    assert(!v::v311::recover_exact_owner_from_live_parsers(
        model,7,live.material,live,&model_lookup,rejected));
    std::cout<<"V311_EXACT_LIVE_MODEL_AND_MTD_SOURCE_OWNER_GUARDS_PASS\n";
}
