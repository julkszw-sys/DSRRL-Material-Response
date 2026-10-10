#pragma once
// v203 opt-in: exactly the SAME 36 original equipment rows as the canonical
// read-only manifest, predecoded at compile time. No mutable cache, mutex,
// epoch, borrowed COM pointer or new material authority.
//
// IMPORTANT: The original slow verifier remains available and is the source
// of truth. This specialization must be bit-identical for all queries.
// The stock consumer / PTDE t10 guard still independently validates resource
// identity, actual MTD SHA, receiver and native shader before activation.
#include "dsrrl/runtime/v203_equipment_flver_slot_spec_manifest.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime::v203_equipment::constexpr_lookup {

struct compiled_slot {
    core::sha256_digest flver_sha256{};
    std::uint32_t slot = 0u;
    std::uint64_t mtd_semantic_hash = 0u;
    std::uint64_t logical_spec_hash = 0u;
};

constexpr int digit(char c) noexcept {
    return (c >= '0' && c <= '9') ? (c - '0')
         : (c >= 'a' && c <= 'f') ? (c - 'a' + 10)
         : (c >= 'A' && c <= 'F') ? (c - 'A' + 10) : -1;
}

constexpr bool valid_digest_literal(const char *hex) noexcept {
    if (!hex) return false;
    for (std::size_t i = 0u; i < 64u; ++i)
        if (digit(hex[i]) < 0) return false;
    return hex[64] == '\0';
}

constexpr core::sha256_digest decode_sha256(const char *hex) noexcept {
    core::sha256_digest out{};
    if (!valid_digest_literal(hex)) return out;
    for (std::size_t i = 0u; i < out.size(); ++i)
        out[i] = static_cast<std::uint8_t>(
            (digit(hex[i * 2u]) << 4) | digit(hex[i * 2u + 1u]));
    return out;
}

// Exactly matches mtd_semantic_hash in mtd_semantic_census.cpp (original
// byte sequence, case-sensitive) and logical_spec_hash in the manifest
// (ASCII lower-case of the stock logical g_Specular name).
constexpr std::uint64_t fnv1a(
    const char *text, bool ascii_casefold = false) noexcept {
    if (!text) return ascii_casefold ? 14695981039346656037ull : 0ull;
    std::uint64_t hash = 14695981039346656037ull;
    for (; *text; ++text) {
        std::uint8_t byte = static_cast<std::uint8_t>(*text);
        if (ascii_casefold && byte >= 'A' && byte <= 'Z')
            byte = static_cast<std::uint8_t>(byte + ('a' - 'A'));
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

constexpr bool manifest_digest_literals_valid() noexcept {
    for (const auto &r : k_proven_parts_specs)
        if (!valid_digest_literal(r.flver_sha256))
            return false;
    return true;
}

static_assert(manifest_digest_literals_valid(),
    "Cannot optimize provenance: one original SHA256 literal is malformed");

constexpr auto compile_manifest() noexcept {
    std::array<compiled_slot, 36u> out{};
    for (std::size_t i = 0u; i < out.size(); ++i) {
        const auto &old = k_proven_parts_specs[i];
        out[i] = {decode_sha256(old.flver_sha256),old.slot,
                  fnv1a(old.mtd),fnv1a(old.spec,true)};
    }
    return out;
}

inline constexpr auto k_exact_compiled_slots = compile_manifest();
static_assert(k_exact_compiled_slots.size() == 36u,
    "Missing provenance: must preserve all 36 original equipment tuples");

// Same authoritative owner verification as the baseline function,
// followed by identical full-FLVER-SHA + exact slot + exact MTD hash match.
// No 64-bit owner token may substitute for the 32-byte SHA.
inline bool exact_proven_owner_slot_fast(
    const operators::material_response::mtd_semantic_query &query) noexcept {
    const auto &m = query.material;
    if (!m.valid || !m.owner_tuple_exact ||
        !m.actual_material_exact || !m.material_slot_valid ||
        !operators::material_response::
            has_exact_flver_material_ownership(query))
        return false;
    for (const auto &r : k_exact_compiled_slots)
        if (r.slot == m.material_slot &&
            r.mtd_semantic_hash == m.semantic_name_hash &&
            r.flver_sha256 == m.flver_sha256)
            return true;
    return false;
}

inline bool exact_proven_slot_spec_hash_fast(
    const operators::material_response::mtd_semantic_query &query,
    std::uint64_t stock_logical_spec_hash) noexcept {
    const auto &m = query.material;
    if (!stock_logical_spec_hash || !m.valid ||
        !m.owner_tuple_exact || !m.actual_material_exact ||
        !m.material_slot_valid ||
        !operators::material_response::
            has_exact_flver_material_ownership(query))
        return false;
    for (const auto &r : k_exact_compiled_slots)
        if (r.slot == m.material_slot &&
            r.mtd_semantic_hash == m.semantic_name_hash &&
            r.logical_spec_hash == stock_logical_spec_hash &&
            r.flver_sha256 == m.flver_sha256)
            return true;
    return false;
}

} // namespace dsrrl::runtime::v203_equipment::constexpr_lookup
