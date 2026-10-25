#pragma once
// Exact, read-only player-equipment FLVER/slot g_Specular provenance.
// From DSRRL_FINAL_EVIDENCE_PART04(2).zip, SHA256:
// a76b13c909fb4a30a1a312cba7d594a851193bef55e578934829fd873f872c97
// 151 DCX archive inputs parsed structurally (158 FLVER2, 244 slots).
// This manifest covers only independently verified equipment parts.
// Unknown FLVER/slot names fail open. No native-view pointer heuristics.
// This proves asset provenance, NOT consumer/pixel equivalence.
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include <cstddef>
#include <cstdint>
namespace dsrrl::runtime::v203_equipment {
struct flver_slot_spec_row {
    const char *flver_sha256;
    std::uint32_t slot;
    const char *mtd;
    const char *spec;
};
inline constexpr flver_slot_spec_row k_proven_parts_specs[] = {
    { "bd86fb820aea2ec7648b7924a560ec2cd6d4b50c3d2c1d242df514b01f1c1486", 0u, "P_DullLeather[DSB].mtd", "AM_F_9450_L_s" },
    { "bd86fb820aea2ec7648b7924a560ec2cd6d4b50c3d2c1d242df514b01f1c1486", 1u, "P_DullLeather[DSB].mtd", "AM_F_9450_L_s" },
    { "bd86fb820aea2ec7648b7924a560ec2cd6d4b50c3d2c1d242df514b01f1c1486", 2u, "P_DullLeather[DSB].mtd", "AM_F_9450_L_s" },
    { "7a43cf6d83dfd30000048a7a959ce7d4a5b1aa2f7fac8e37a0bcd23591ba27d6", 0u, "P_DullLeather[DSB].mtd", "AM_F_9450_s" },
    { "7a43cf6d83dfd30000048a7a959ce7d4a5b1aa2f7fac8e37a0bcd23591ba27d6", 1u, "P_DullLeather[DSB].mtd", "AM_F_9450_s" },
    { "7a43cf6d83dfd30000048a7a959ce7d4a5b1aa2f7fac8e37a0bcd23591ba27d6", 2u, "P_DullLeather[DSB].mtd", "AM_F_9450_s" },
    { "394cdf33d0e55a2fa9188081f4fa0341a239ed5255ced1e53e260075ea6a892d", 0u, "P_Leather[DSB].mtd", "AM_M_9372_L_s" },
    { "394cdf33d0e55a2fa9188081f4fa0341a239ed5255ced1e53e260075ea6a892d", 1u, "P_Leather[DSB].mtd", "AM_M_9372_L_s" },
    { "8ba8f1f49e371a7942a8aa401c140a42bac4a06781afecca522d685feed71788", 0u, "P_Leather[DSB].mtd", "AM_M_9372_s" },
    { "8ba8f1f49e371a7942a8aa401c140a42bac4a06781afecca522d685feed71788", 1u, "P_Leather[DSB].mtd", "AM_M_9372_s" },
    { "4f4eb5c257e280876b100c4880b3f3a2291d3e39bf1098356b8d737efe7f2f7a", 0u, "P_Metal[DSB].mtd", "WP_A_0100_s" },
    { "4f4eb5c257e280876b100c4880b3f3a2291d3e39bf1098356b8d737efe7f2f7a", 1u, "P_DullLeather[DSB].mtd", "WP_A_0100_s" },
    { "146864017ebee2230cc10c629cc83d2a58a5864b436412664956645e94766d56", 0u, "P_DullLeather[DSB].mtd", "WP_A_0100_s" },
    { "1d40adc223050ade28b2d65cf99671e3a401b65fb992e39959a821336f2f30a1", 0u, "P_Metal[DSB].mtd", "WP_A_0103_s" },
    { "c1fc6e9fce3669fb511c461292c7e80d9cc92f2c5ced664f74b7d6d3d2345fb8", 0u, "P_Metal[DSB].mtd", "WP_A_0106_mailbreaker_s" },
    { "c1fc6e9fce3669fb511c461292c7e80d9cc92f2c5ced664f74b7d6d3d2345fb8", 1u, "P_Leather[DSB].mtd", "WP_A_0106_mailbreaker_s" },
    { "915f6ef1a5aa0194fa99f7dde6aab07a1e9aadfd46702ad6db7910d5bc06d73a", 0u, "P_Metal[DSB].mtd", "WP_A_0106_mailbreaker_s" },
    { "915f6ef1a5aa0194fa99f7dde6aab07a1e9aadfd46702ad6db7910d5bc06d73a", 1u, "P_DullLeather[DSB].mtd", "WP_A_0106_mailbreaker_s" },
    { "1d13b3bcca55f05692d156f436a52e1dc024ef2c48434a13fdba0d94a6b7ad0f", 0u, "P_Metal[DSB].mtd", "WP_A_0107_s" },
    { "6785e10268f49f3b8124be707243af770e149283ba2335ca86e6f6d4f32a5342", 0u, "P_Metal[DSB].mtd", "WP_A_0107_s" },
    { "57b0fcebd70100d02599f9a75fa8b6deb127bdcdf6a4a7e531b1990ebc8cb47b", 0u, "P_Metal[DSB].mtd", "WP_A_0211_s" },
    { "0efc45b1f9e8f56eb78bde96500f1185c39cf258972333f9b9a3adbed2083c36", 0u, "P_Metal[DSB].mtd", "WP_A_0652_L_s" },
    { "99845252bde69c71721dbd8f8a744900d9773af724c6a8d1ebaa201df1722186", 0u, "P_DullLeather[DSB].mtd", "WP_A_0658_s" },
    { "99845252bde69c71721dbd8f8a744900d9773af724c6a8d1ebaa201df1722186", 1u, "P_Metal[DSB].mtd", "WP_A_0658_s" },
    { "2a60658dbcbe85d32706c575f861528be2e18f5079c792ae71d7bfef314d5405", 0u, "P_Metal[DSB].mtd", "WP_A_0801_Scythe_s" },
    { "2a60658dbcbe85d32706c575f861528be2e18f5079c792ae71d7bfef314d5405", 1u, "P_DullLeather[DSB].mtd", "WP_A_0801_Scythe_s" },
    { "08ed76b084d264f5c0598f822666ee25c6c2ffd1b0e20378f24866c2672eec1d", 0u, "P_Leather[DSB].mtd", "WP_A_0905_s" },
    { "08ed76b084d264f5c0598f822666ee25c6c2ffd1b0e20378f24866c2672eec1d", 1u, "P_Leather[DSB].mtd", "WP_A_0905_s" },
    { "08ed76b084d264f5c0598f822666ee25c6c2ffd1b0e20378f24866c2672eec1d", 2u, "P_Metal[DSB].mtd", "WP_A_0905_s" },
    { "39fb74429c50f693b95ad2702a07533eeb46acf407e154d1e186bd8c82847030", 0u, "P_Leather[DSB].mtd", "WP_A_0906_s" },
    { "39fb74429c50f693b95ad2702a07533eeb46acf407e154d1e186bd8c82847030", 1u, "P_Leather[DSB].mtd", "WP_A_0906_s" },
    { "e74a0c553b158b6eccc24f51646d1a3a132ec3faa639d700d180ee0b278d5293", 0u, "P_Metal[DSB].mtd", "WP_A_1100_WingedSpear_s" },
    { "635b529de8a8d29b42caeee01b5597a00e2e6c9b073d3ab3c77e0ef9e69c4fda", 0u, "P_Leather[DSB].mtd", "WP_A_1200_s" },
    { "3f6b4efa282895894f2d333d382f91884519c3b30d41838987ff1589d62e6776", 0u, "P_Leather[DSB].mtd", "WP_A_1201_s" },
    { "e7f121b43b9352db24c28d2d7819cd43a1c81e851c9ee98fce1b02332eb92315", 0u, "P_Leather[DSB].mtd", "WP_A_1700_s" },
    { "e7f121b43b9352db24c28d2d7819cd43a1c81e851c9ee98fce1b02332eb92315", 1u, "P_DullLeather[DSB].mtd", "WP_A_1700_s" },
    // 2026-10-24 DSR parts/HD_{M,F}_9360 FLVER slot0 source-joined
    // to matching PTDE g_Specular name+MTD. This is provenance ONLY:
    // standalone PTDE consumer/stock SRV/receiver/asset readiness remain
    // mandatory. It is not a C_Metal or Edge family authorization.
    { "bfbef63e9611bd8b753f8197e5cabeabbf08ec400e098f2191561bdc5ce7396b", 0u, "C_Metal[DSB].mtd", "hd_m_9360_s" },
    { "59c937ced4aaf41ba3f6db4a351d47b41dabd176224ecca4123b72a11d23baee", 0u, "P_Metal[DSB]_Edge.mtd", "hd_f_9360_s" },
};
static_assert(sizeof(k_proven_parts_specs) /
    sizeof(k_proven_parts_specs[0]) == 38u);
inline int hex_digit(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
inline bool digest_matches(const core::sha256_digest &got,
                           const char *hex) noexcept {
    for (std::size_t i = 0u; i < got.size(); ++i) {
        const auto hi = hex_digit(hex[2u*i]);
        const auto lo = hex_digit(hex[2u*i+1u]);
        if (hi < 0 || lo < 0 ||
            got[i] != static_cast<std::uint8_t>((hi << 4) | lo))
            return false;
    }
    return hex[got.size()*2u] == 0;
}
inline bool same_ascii_casefold(const char *a,const char *b) noexcept {
    if (!a || !b) return false;
    while (*a && *b) {
        const auto lc = [](char c) noexcept {
            return c >= 'A' && c <= 'Z' ? char(c + ('a'-'A')) : c;
        };
        if (lc(*a++) != lc(*b++)) return false;
    }
    return *a == 0 && *b == 0;
}
inline bool exact_proven_slot_spec(
    const operators::material_response::mtd_semantic_query &query,
    const char *native_specular_name) noexcept {
    const auto &m = query.material;
    if (!native_specular_name || !*native_specular_name ||
        !m.valid || !m.owner_tuple_exact || !m.actual_material_exact ||
        !m.material_slot_valid ||
        !operators::material_response::
            has_exact_flver_material_ownership(query))
        return false;
    for (const auto &r : k_proven_parts_specs) {
        if (r.slot == m.material_slot &&
            m.semantic_name_hash ==
              operators::material_response::mtd_semantic_hash(r.mtd) &&
            same_ascii_casefold(native_specular_name,r.spec) &&
            digest_matches(m.flver_sha256,r.flver_sha256))
            return true;
    }
    return false;
}

inline std::uint64_t logical_spec_hash(const char *name) noexcept {
    std::uint64_t h = 14695981039346656037ull;
    if (!name) return h;
    for (; *name; ++name) {
        std::uint32_t c = static_cast<unsigned char>(*name);
        if (c >= 'A' && c <= 'Z') c += 32u;
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ull;
    }
    return h;
}
// Negative provenance stages are diagnostics only; none authorizes t10.
enum class owner_slot_probe : std::uint8_t {
    authority_missing = 0,
    flver_not_in_corpus,
    slot_not_in_corpus,
    mtd_not_in_corpus,
    exact
};
inline owner_slot_probe classify_exact_owner_slot(
    const operators::material_response::mtd_semantic_query &query) noexcept {
    const auto &m = query.material;
    if (!m.valid || !m.owner_tuple_exact ||
        !m.actual_material_exact || !m.material_slot_valid ||
        !operators::material_response::
            has_exact_flver_material_ownership(query))
        return owner_slot_probe::authority_missing;
    bool sha_found = false;
    bool slot_found = false;
    for (const auto &r : k_proven_parts_specs) {
        if (!digest_matches(m.flver_sha256,r.flver_sha256))
            continue;
        sha_found = true;
        if (r.slot != m.material_slot)
            continue;
        slot_found = true;
        if (m.semantic_name_hash ==
              operators::material_response::mtd_semantic_hash(r.mtd))
            return owner_slot_probe::exact;
    }
    return !sha_found ? owner_slot_probe::flver_not_in_corpus
         : !slot_found ? owner_slot_probe::slot_not_in_corpus
                       : owner_slot_probe::mtd_not_in_corpus;
}
// Read-only provenance stage independent of the stock t1 name-cache hit.
// This proves only the FLVER owner, material slot and exact MTD,
// never authorizes replacement or infers a bound resource.
inline bool exact_proven_owner_slot(
    const operators::material_response::mtd_semantic_query &query) noexcept {
    const auto &m = query.material;
    if (!m.valid || !m.owner_tuple_exact ||
        !m.actual_material_exact || !m.material_slot_valid ||
        !operators::material_response::
            has_exact_flver_material_ownership(query))
        return false;
    for (const auto &r : k_proven_parts_specs) {
        if (r.slot == m.material_slot &&
            m.semantic_name_hash ==
              operators::material_response::mtd_semantic_hash(r.mtd) &&
            digest_matches(m.flver_sha256,r.flver_sha256))
            return true;
    }
    return false;
}
// Domain cut: one exact FLVER SHA, one material slot, matching MTD,
// and that slot's own g_Specular name (not a merely allowlisted texture).
// No D3D view identity, unrelated materials or fallback name guessing.
inline bool exact_proven_slot_spec_hash(
    const operators::material_response::mtd_semantic_query &query,
    std::uint64_t stock_logical_spec_hash) noexcept {
    const auto &m = query.material;
    if (!stock_logical_spec_hash || !m.valid ||
        !m.owner_tuple_exact || !m.actual_material_exact ||
        !m.material_slot_valid ||
        !operators::material_response::
            has_exact_flver_material_ownership(query))
        return false;
    for (const auto &r : k_proven_parts_specs) {
        if (r.slot == m.material_slot &&
            m.semantic_name_hash ==
              operators::material_response::mtd_semantic_hash(r.mtd) &&
            logical_spec_hash(r.spec) == stock_logical_spec_hash &&
            digest_matches(m.flver_sha256,r.flver_sha256))
            return true;
    }
    return false;
}
} // namespace dsrrl::runtime::v203_equipment
