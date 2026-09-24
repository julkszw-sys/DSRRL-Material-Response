#include "dsrrl/operators/legacy_plan/a1_mask_decomposition.hpp"

namespace dsrrl::operators::legacy_plan {
namespace {

enum class bit_state : std::uint8_t {
    closed = 0,
    nonclosed,
    rejected
};

struct bit_owner {
    std::uint32_t bit;
    core::operator_id owner;
    bit_state state;
    bool has_owner;
};

constexpr bit_owner k_map[] = {
    {1u, core::operator_id::terminal_sat_rgb, bit_state::closed, true},
    {2u, core::operator_id::diffuse_material_domain, bit_state::closed, true},
    {4u, core::operator_id::pointlight_pnts_attenuation, bit_state::closed, true},
    {8u, core::operator_id::terminal_sat_rgb, bit_state::closed, true},
    {16u, core::operator_id::material_response, bit_state::rejected, false},
    {32u, core::operator_id::envspec_nospc_delete, bit_state::closed, true},
    {64u, core::operator_id::diffuse_material_domain, bit_state::closed, true},
    {128u, core::operator_id::fixed_postfog_identity, bit_state::closed, true},
    {256u, core::operator_id::terminal_sat_rgb, bit_state::closed, true},
    {512u, core::operator_id::terminal_sat_rgb, bit_state::nonclosed, true},
    {1024u, core::operator_id::diffuse_material_domain, bit_state::nonclosed, true},
    {2048u, core::operator_id::terminal_sat_rgb, bit_state::nonclosed, true},
    {4096u, core::operator_id::envspec_pmetal_diagnostic, bit_state::rejected, true},
    {8192u, core::operator_id::terminal_sat_rgb, bit_state::nonclosed, true},
    {16384u, core::operator_id::terminal_sat_rgba, bit_state::rejected, true}
};

bool contains(const decomposition &d, core::operator_id id) noexcept
{
    for (std::uint8_t i = 0; i < d.owner_count; ++i)
        if (d.owners[i] == id)
            return true;
    return false;
}

} // namespace

decomposition decompose_p22_mask(std::uint32_t legacy_mask) noexcept
{
    decomposition out;
    std::uint32_t catalog_bits = 0;

    for (const auto &entry : k_map) {
        catalog_bits |= entry.bit;
        if ((legacy_mask & entry.bit) == 0)
            continue;

        switch (entry.state) {
        case bit_state::closed: out.closed_bits |= entry.bit; break;
        case bit_state::nonclosed: out.nonclosed_bits |= entry.bit; break;
        case bit_state::rejected: out.rejected_bits |= entry.bit; break;
        }

        if (entry.has_owner &&
            !contains(out, entry.owner) &&
            out.owner_count < out.owners.size())
            out.owners[out.owner_count++] = entry.owner;
    }

    out.unknown_bits = legacy_mask & ~catalog_bits;
    return out;
}

std::uint32_t legacy_bits_for(core::operator_id op) noexcept
{
    std::uint32_t bits = 0;
    for (const auto &entry : k_map)
        if (entry.has_owner && entry.owner == op)
            bits |= entry.bit;
    return bits;
}

} // namespace dsrrl::operators::legacy_plan
