#pragma once
#include <cstdint>

namespace dsrrl::runtime::pmetal_donor_provenance {
enum class row_origin : std::uint8_t {
    ptde_exact = 0,
    dsr_only = 1,
    invalid = 2
};
// Never extrapolate the tail of a source-complete PTDE donor list.
// The exact original DSR LightBank code owns an unmapped row.
constexpr row_origin classify(
    std::uint32_t row,
    std::uint32_t live_count,
    std::uint32_t donor_count) noexcept
{
    if (live_count == 0u || donor_count > live_count ||
        row >= live_count)
        return row_origin::invalid;
    return row < donor_count
        ? row_origin::ptde_exact
        : row_origin::dsr_only;
}
} // namespace dsrrl::runtime::pmetal_donor_provenance
