#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>

namespace dsrrl::operators::dof::ptde_bank {

struct encoded_row {
    std::uint32_t far_begin_bits;
    std::uint32_t far_end_bits;
    std::uint8_t far_mul;
    std::uint32_t near_begin_bits;
    std::uint32_t near_end_bits;
    std::uint8_t near_mul;
    std::uint32_t dispersion_sq_bits;
};

struct decoded_state { float far_begin, far_end, far_mul, near_begin, near_end, near_mul, dispersion_sq; };

inline constexpr std::array<std::uint8_t,9> area_ids = {10u,11u,12u,13u,14u,15u,16u,17u,18u};
inline constexpr std::array<std::uint64_t,9> dsr_ptde_structural_signatures = {
    0x96ff83e97066616fULL,
    0x4369cfd16580100bULL,
    0xcf06079b3d769e22ULL,
    0x8495ff3b666052a9ULL,
    0x5cefc2fa3d4e63d2ULL,
    0x23e4ac6187d0387bULL,
    0x5b8ca0a616a3898bULL,
    0x6e971c759010210dULL,
    0x600fa9b5f6e6446dULL,
};

inline constexpr std::array<encoded_row,221> unique_rows = {{
#include "ptde_dofbank_rows_0.inc"
#include "ptde_dofbank_rows_1.inc"
#include "ptde_dofbank_rows_2.inc"
#include "ptde_dofbank_rows_3.inc"
}};

inline constexpr std::array<std::uint8_t,576> row_dictionary_index = {{
#include "ptde_dofbank_indices.inc"
}};

inline float bits_to_float(std::uint32_t bits) noexcept { float v=0.0f; std::memcpy(&v,&bits,sizeof(v)); return v; }

inline int area_slot_from_signature(std::uint64_t signature) noexcept {
    for (std::size_t i=0;i<dsr_ptde_structural_signatures.size();++i) if (dsr_ptde_structural_signatures[i]==signature) return static_cast<int>(i);
    return -1;
}

inline const encoded_row *row_by_slot(std::size_t area_slot,std::uint32_t selector) noexcept {
    if(area_slot>=area_ids.size() || selector>=64u) return nullptr;
    const auto dict=row_dictionary_index[area_slot*64u+selector];
    return dict<unique_rows.size()?&unique_rows[dict]:nullptr;
}

inline decoded_state decode(const encoded_row &r) noexcept {
    return {bits_to_float(r.far_begin_bits),bits_to_float(r.far_end_bits),static_cast<float>(r.far_mul),bits_to_float(r.near_begin_bits),bits_to_float(r.near_end_bits),static_cast<float>(r.near_mul),bits_to_float(r.dispersion_sq_bits)};
}

inline bool blend(std::size_t area_a,std::uint32_t row_a,std::size_t area_b,std::uint32_t row_b,float beta,decoded_state &out) noexcept {
    const auto *ra=row_by_slot(area_a,row_a); const auto *rb=row_by_slot(area_b,row_b);
    if(!ra || !rb || !std::isfinite(beta)) return false;
    const auto a=decode(*ra), b=decode(*rb);
    const auto l=[beta](float x,float y){return x+(y-x)*beta;};
    out={l(a.far_begin,b.far_begin),l(a.far_end,b.far_end),l(a.far_mul,b.far_mul),l(a.near_begin,b.near_begin),l(a.near_end,b.near_end),l(a.near_mul,b.near_mul),l(a.dispersion_sq,b.dispersion_sq)};
    return std::isfinite(out.far_begin)&&std::isfinite(out.far_end)&&std::isfinite(out.far_mul)&&std::isfinite(out.near_begin)&&std::isfinite(out.near_end)&&std::isfinite(out.near_mul)&&std::isfinite(out.dispersion_sq);
}

} // namespace dsrrl::operators::dof::ptde_bank
