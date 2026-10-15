#pragma once

// V296 operator-local material policy. This guard is deliberately NOT an
// authorization of an unknown PTDE consumer and does not classify by MTD.
// A live stock t1 identity may be pinned to the 2026-10-10 SpecRGB manifest,
// yet the generic Material Response tail is diffuse-only: combining that
// partial PTDE material result with stock DSR PBL would be a hybrid.
// Other independently established operators (e.g. Upper/Lower) are NOT gated.
namespace dsrrl::runtime::v296 {

enum class material_atomic_policy : unsigned char {
    independent_operator,       // No proven October SpecRGB draw membership.
    exact_ptde_island,          // Existing receiver/owner-certified transaction.
    stock_material_fail_open    // October SpecRGB, no certified PTDE consumer.
};

// A missing/corrupt sidecar must NOT clear stock_material_fail_open: the
// logical manifest membership belongs to the stock DSR t1 identity, not to
// success or failure of loading a PTDE DDS file.
//
// exact_island_authorized is a verified draw-local consumer/material gate,
// NOT merely presence of SpecRGB and not a guessed "_l_s" <-> "_s" alias.
constexpr material_atomic_policy decide_material_policy(
    bool exact_island_authorized,
    bool stock_t1_bound,
    bool logical_name_resolved,
    bool oct10_manifest_member) noexcept
{
    if (exact_island_authorized)
        return material_atomic_policy::exact_ptde_island;

    if (stock_t1_bound &&
        logical_name_resolved &&
        oct10_manifest_member)
        return material_atomic_policy::stock_material_fail_open;

    return material_atomic_policy::independent_operator;
}

constexpr bool suppress_partial_material_response(
    material_atomic_policy policy) noexcept
{
    return policy == material_atomic_policy::stock_material_fail_open;
}

} // namespace dsrrl::runtime::v296
