#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// Shared, source-preimage-guarded PTDE c100 material-factor cut.
// Consumer authorization is deliberately separate: this patch is not
// permission to pair PTDE SpecRGB with stock DSR PBL/F0.
namespace dsrrl::operators::material_response {

enum class workflow_c100_patch_result : std::uint8_t {
    applied,
    invalid_site,
    nonhomologous_preimage,
    carrier_alias
};

inline workflow_c100_patch_result apply_ptde_workflow_c100_mad(
    std::vector<std::uint32_t> &words,
    std::size_t mad_word,
    std::uint32_t material_register,
    std::uint32_t diffuse_carrier_register) noexcept
{
    using result = workflow_c100_patch_result;
    if (mad_word > words.size() ||
        words.size() - mad_word < 10u)
        return result::invalid_site;

    // The producer's output must not alias its downstream diffuse carrier.
    if (material_register == diffuse_carrier_register)
        return result::carrier_alias;

    // Exact DSR MaterialWorkflow:
    // mad rMaterial.xyz, W.yyyy, rMaterial.xyz, b12[1].xyz
    // Requires the caller to have verified the surrounding IF/delta/MUL
    // sequence and the stock receiver's exact DXBC identity.
    if (words[mad_word + 1u] != 0x00100072u ||
        words[mad_word + 2u] != material_register ||
        words[mad_word + 3u] != 0x00100556u ||
        words[mad_word + 5u] != 0x00100246u ||
        words[mad_word + 6u] != material_register ||
        words[mad_word + 7u] != 0x00208246u ||
        words[mad_word + 8u] != 12u ||
        words[mad_word + 9u] != 1u)
        return result::nonhomologous_preimage;

    // The scalar register operand and immediate operand are both 2 DWORDs.
    // This is length preserving: select exact PTDE b12[1] c100, leaving
    // the diffuse texture, COLOR0 and all downstream operators untouched.
    words[mad_word + 3u] = 0x00004001u;
    words[mad_word + 4u] = 0x00000000u;
    return result::applied;
}
} // namespace dsrrl::operators::material_response
