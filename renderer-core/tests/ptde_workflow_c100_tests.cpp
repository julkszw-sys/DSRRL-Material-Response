#include "dsrrl/operators/material_response/ptde_workflow_c100.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <vector>

using dsrrl::operators::material_response::apply_ptde_workflow_c100_mad;
using dsrrl::operators::material_response::workflow_c100_patch_result;

namespace {
constexpr std::size_t k_mad = 12u;
constexpr std::uint32_t k_material = 7u;
constexpr std::uint32_t k_diffuse = 4u;

std::vector<std::uint32_t> source_words()
{
    std::vector<std::uint32_t> words(36u, 0xdeadbeefu);
    words[k_mad + 0u] = 0x0a000032u; // MAD opcode and size
    words[k_mad + 1u] = 0x00100072u;
    words[k_mad + 2u] = k_material;
    words[k_mad + 3u] = 0x00100556u;
    words[k_mad + 4u] = 3u; // live workflow scalar source
    words[k_mad + 5u] = 0x00100246u;
    words[k_mad + 6u] = k_material;
    words[k_mad + 7u] = 0x00208246u;
    words[k_mad + 8u] = 12u;
    words[k_mad + 9u] = 1u;
    return words;
}
} // namespace

int main()
{
    using result = workflow_c100_patch_result;
    auto words = source_words();
    const auto original = words;
    assert(apply_ptde_workflow_c100_mad(
        words, k_mad, k_material, k_diffuse) == result::applied);
    assert(words.size() == original.size());
    for (std::size_t i = 0u; i < words.size(); ++i) {
        if (i == k_mad + 3u) {
            assert(words[i] == 0x00004001u);
        } else if (i == k_mad + 4u) {
            assert(words[i] == 0x00000000u);
        } else {
            assert(words[i] == original[i]);
        }
    }
    // A completed cut cannot be accidentally applied a second time.
    assert(apply_ptde_workflow_c100_mad(
        words, k_mad, k_material, k_diffuse) ==
        result::nonhomologous_preimage);

    auto alias = source_words();
    assert(apply_ptde_workflow_c100_mad(
        alias, k_mad, k_material, k_material) ==
        result::carrier_alias);
    assert(alias == original);

    auto invalid = source_words();
    invalid[k_mad + 8u] = 9u; // not the PTDE b12 carrier
    const auto invalid_before = invalid;
    assert(apply_ptde_workflow_c100_mad(
        invalid, k_mad, k_material, k_diffuse) ==
        result::nonhomologous_preimage);
    assert(invalid == invalid_before);

    auto truncated = source_words();
    assert(apply_ptde_workflow_c100_mad(
        truncated, truncated.size() - 9u,
        k_material, k_diffuse) == result::invalid_site);
    assert(truncated == original);

    assert(apply_ptde_workflow_c100_mad(
        truncated, truncated.size() + 1u,
        k_material, k_diffuse) == result::invalid_site);
    assert(truncated == original);
    return 0;
}
