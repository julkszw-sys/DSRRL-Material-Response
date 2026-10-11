#pragma once
// Experimental (PR304 descendant): P_Metal-style exact-source owner authority
// for six PR304-observed P_Leather[DSB].mtd equipment slots only.
//
// NO actual_material_exact flag shortcut for unrelated materials:
// FLVER complete SHA-256, exact slot, immutable MTD name + FULL raw MTD SHA,
// material family, direct-exact route tag and same-draw stock gSpec hash.
// The full owner SHA must match the source-complete FLVER owner corpus.
// Source full-SHA candidates were identified from logged SHA4 by unique
// corpus lookup; at runtime this check uses the ACTUAL full 32-byte SHA.
// DullLeather, metals and all unlisted slots remain stock DSR.
// Sidecar presence, actual stock D3D11 t1, shader receiver and keyed PTDE
// source are independent downstream checks; this header authorizes none.
// PTDE-visible pixel equivalence remains OPEN.
#include "dsrrl/runtime/v203_equipment_constexpr_lookup.hpp"
#include <array>
#include <cstdint>
#include <cstddef>

namespace dsrrl::runtime::v203_equipment::live_leather {
namespace mr = operators::material_response;
namespace dec = constexpr_lookup;

struct verified_observed_slot {
    core::sha256_digest flver{};
    std::uint32_t slot = 0u;
    std::uint64_t stock_gspec = 0ull;
};
inline constexpr std::uint64_t k_leather_name =
    dec::fnv1a("P_Leather[DSB].mtd");
inline constexpr std::uint64_t k_difspcbmp =
    dec::fnv1a("DifSpcBmp");
inline constexpr core::sha256_digest k_leather_raw_mtd =
    dec::decode_sha256("0f2b9a1012b83c4dfc909c311d29a11147de8c15191bc62398ce0d5e1cc6757e");
// This is the exact DIRECT_EXACT material route observed in PR304.
// Do not interpret 0x4000002d as a parameter or shader receiver ID.
inline constexpr std::uint32_t k_leather_route = 1073741869u;
inline constexpr std::array<verified_observed_slot,6u> k_slots{{
    {dec::decode_sha256("39232650842756828e81b8799e2dd7f5222b1205e8af2effdf22942ccb9d9cac"),1u,dec::fnv1a("BD_M_9430_2_s",true)},
    {dec::decode_sha256("39232650842756828e81b8799e2dd7f5222b1205e8af2effdf22942ccb9d9cac"),2u,dec::fnv1a("BD_M_9430_1_s",true)},
    {dec::decode_sha256("39232650842756828e81b8799e2dd7f5222b1205e8af2effdf22942ccb9d9cac"),3u,dec::fnv1a("BD_M_9430_1_s",true)},
    {dec::decode_sha256("39232650842756828e81b8799e2dd7f5222b1205e8af2effdf22942ccb9d9cac"),4u,dec::fnv1a("BD_M_9430_2_s",true)},
    {dec::decode_sha256("3d5571cd6c19130df9fc38aab5fc3b50bca91812ad0810806f0c718a4fe0d552"),0u,dec::fnv1a("LG_M_9430_s",true)},
    {dec::decode_sha256("b86e7c235d769c4d112d8b862deb4ae1b631da0990d8ac9a3fc3ead36b4bfaf2"),0u,dec::fnv1a("AM_M_9430_s",true)},
}};
constexpr bool unique_tuples() noexcept {
    for (std::size_t a=0;a<k_slots.size();++a)
        for (std::size_t b=a+1;b<k_slots.size();++b)
            if (k_slots[a].flver==k_slots[b].flver &&
                k_slots[a].slot==k_slots[b].slot) return false;
    return true;
}
static_assert(unique_tuples(), "Ambiguous Leather FLVER/slot");
static_assert(k_slots.size()==6u, "Leather source-exact experiment scope drift");
static_assert(k_leather_name==0xabbcca95a57070c3ull, "MTD semantic hash drift");
static_assert(k_difspcbmp==0x62fa441bb6bc7583ull, "Material family hash drift");

// Equivalent authorization strength to the original P_Metal exact source
// predicate in material/owner identity, NOT equivalent rendered behavior.
inline bool exact_material(const mr::material_identity &m) noexcept {
    if (!m.valid || !m.owner_tuple_exact ||
        !m.material_slot_valid ||
        m.semantic_name_hash != k_leather_name ||
        m.material_family_hash != k_difspcbmp ||
        m.route_index != k_leather_route ||
        m.raw_mtd_sha256 != k_leather_raw_mtd)
        return false;
    for (const auto &slot : k_slots)
        if (slot.slot==m.material_slot &&
            slot.flver==m.flver_sha256)
            return true;
    return false;
}

inline bool exact_owner_slot(const mr::mtd_semantic_query &q) noexcept {
    if (!exact_material(q.material) || !q.ownership.exact ||
        !q.ownership.material_slot_valid ||
        q.ownership.material_slot!=q.material.material_slot ||
        q.ownership.flver_sha256!=q.material.flver_sha256 ||
        !mr::has_exact_flver_material_ownership(q))
        return false;
    return true;
}

inline bool exact_owner_slot_spec(
    const mr::mtd_semantic_query &q,
    std::uint64_t live_stock_t1_name_hash) noexcept {
    if (!live_stock_t1_name_hash || !exact_owner_slot(q))
        return false;
    for (const auto &slot : k_slots)
        if (slot.flver==q.material.flver_sha256 &&
            slot.slot==q.material.material_slot &&
            slot.stock_gspec==live_stock_t1_name_hash)
            return true;
    return false;
}
} // namespace dsrrl::runtime::v203_equipment::live_leather
