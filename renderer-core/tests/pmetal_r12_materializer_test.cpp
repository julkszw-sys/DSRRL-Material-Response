#include "dsrrl/core/feature_registry.hpp"
#include "dsrrl/core/types.hpp"
#include "dsrrl/operators/env_spec/pmetal_rgba_materializer.hpp"
#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/lightbank/upper_lower_hemenv_materializer.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>


// The R12 lineage physically keeps Upper/Lower disabled and the test always
// calls the P_Metal materializer with compose_upper_lower=false. Provide the
// dormant symbol locally so this portable harness does not need to build the
// unrelated U/L generator/materializer stack.
namespace dsrrl::operators::lightbank {
upper_lower_hemenv_materialize_outcome
augment_upper_lower_hemenv_verified_base(
    const std::uint8_t *,
    std::size_t,
    const std::uint8_t *,
    std::size_t,
    std::uint32_t,
    std::vector<std::uint8_t> &) noexcept
{
    return {};
}
} // namespace dsrrl::operators::lightbank

namespace {
std::vector<std::uint8_t> read_file(const char *path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    if (size <= 0) return {};
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    return file ? bytes : std::vector<std::uint8_t>{};
}

std::uint32_t load_u32(const std::uint8_t *p)
{
    return static_cast<std::uint32_t>(p[0]) |
        (static_cast<std::uint32_t>(p[1]) << 8u) |
        (static_cast<std::uint32_t>(p[2]) << 16u) |
        (static_cast<std::uint32_t>(p[3]) << 24u);
}

bool material_workflow_if_absent(
    const std::vector<std::uint8_t> &bytes)
{
    if (bytes.size() < 32u ||
        std::memcmp(bytes.data(), "DXBC", 4u) != 0)
        return false;

    const auto chunk_count =
        load_u32(bytes.data() + 28u);
    if (chunk_count == 0u ||
        32ull + 4ull * chunk_count > bytes.size())
        return false;

    bool code_seen = false;

    for (std::uint32_t c = 0u;
         c < chunk_count;
         ++c) {
        const auto off =
            load_u32(bytes.data() + 32u + c * 4u);
        if (off > bytes.size() ||
            bytes.size() - off < 8u)
            return false;

        const bool code =
            std::memcmp(bytes.data() + off, "SHEX", 4u) == 0 ||
            std::memcmp(bytes.data() + off, "SHDR", 4u) == 0;
        if (!code)
            continue;

        if (code_seen)
            return false;
        code_seen = true;

        const auto payload_size =
            load_u32(bytes.data() + off + 4u);
        if (payload_size > bytes.size() - off - 8u ||
            (payload_size & 3u) != 0u)
            return false;

        const auto *payload = bytes.data() + off + 8u;
        const auto word_count = payload_size / 4u;
        if (word_count < 2u ||
            load_u32(payload + 4u) != word_count)
            return false;

        std::size_t i = 2u;
        while (i < word_count) {
            const auto token =
                load_u32(payload + i * 4u);
            const auto length =
                static_cast<std::size_t>(
                    (token >> 24u) & 0x7fu);
            if (length == 0u ||
                i + length > word_count)
                return false;

            if ((token & 0x7ffu) == 0x1fu &&
                length == 4u &&
                load_u32(payload + (i + 1u) * 4u) == 0x0020800au &&
                load_u32(payload + (i + 2u) * 4u) == 0u &&
                load_u32(payload + (i + 3u) * 4u) == 85u)
                return false;

            i += length;
        }

        if (i != word_count)
            return false;
    }

    return code_seen;
}

bool enable_policy(dsrrl::core::feature_registry &features)
{
    using dsrrl::core::operator_id;
    constexpr std::array<operator_id,12> enabled{{
        operator_id::material_response,
        operator_id::spec_rgb,
        operator_id::diffuse,
        operator_id::normal,
        operator_id::env_spec,
        operator_id::terminal_sat_rgb,
        operator_id::diffuse_material_domain,
        operator_id::point_light,
        operator_id::local_specular_legacy,
        operator_id::pointlight_pnts_attenuation,
        operator_id::envspec_nospc_delete,
        operator_id::fixed_postfog_identity
    }};
    for (const auto op : enabled)
        if (!features.set(op, true))
            return false;
    return
        features.set(operator_id::upper_lower, false) &&
        features.set(operator_id::hemdir3, false) &&
        features.set(operator_id::subsurface, false);
}
} // namespace

int main(int argc, char **argv)
{
    if (argc != 4) {
        std::cerr << "usage: dsrrl_pmetal_r12_offline_test "
                     "<rx33_894.dxbc> <rx34_913.dxbc> <rx35_932.dxbc>\n";
        return 2;
    }

    dsrrl::core::feature_registry features;
    if (!enable_policy(features)) {
        std::cerr << "feature policy setup failed\n";
        return 3;
    }

    constexpr std::array<std::uint32_t,3> receivers{{33u,34u,35u}};
    constexpr std::array<std::uint32_t,3> shader_indices{{894u,913u,932u}};

    for (std::size_t i = 0u; i < receivers.size(); ++i) {
        const auto input = read_file(argv[i + 1u]);
        if (input.empty()) return 10 + static_cast<int>(i);

        std::vector<std::uint8_t> output;
        const auto result =
            dsrrl::operators::env_spec::materialize_pmetal_rgba_receiver(
                features, input.data(), input.size(), false, output);

        using result_t =
            dsrrl::operators::env_spec::pmetal_rgba_materialize_result;

        const bool expected_shadow =
            receivers[i] == 33u || receivers[i] == 34u;

        if (result.result != result_t::applied ||
            result.receiver_id != receivers[i] ||
            result.upper_lower_composed ||
            !result.phn_scene_encoding_composed ||
            result.shadow_visibility_kernel_composed != expected_shadow ||
            !result.spec_rgb_consumer ||
            !result.envdiffuse_linear_consumer_diag ||
            output.empty() ||
            !material_workflow_if_absent(output) ||
            !dsrrl::operators::legacy_plan::dxbc::checksum_container_valid(
                output.data(), output.size())) {
            std::cerr
                << "R12_OFFLINE_FAIL rx=" << receivers[i]
                << " shader=" << shader_indices[i]
                << " result=" << static_cast<unsigned>(result.result)
                << " materialized_rx=" << result.receiver_id
                << " ul=" << result.upper_lower_composed
                << " scene_encoding=" << result.phn_scene_encoding_composed
                << " shadow=" << result.shadow_visibility_kernel_composed
                << " spec=" << result.spec_rgb_consumer
                << " envdiff=" << result.envdiffuse_linear_consumer_diag
                << " output_bytes=" << output.size() << "\n";
            return 20 + static_cast<int>(i);
        }

        std::cout
            << "R12_OFFLINE_APPLIED rx=" << receivers[i]
            << " shader=" << shader_indices[i]
            << " input_bytes=" << input.size()
            << " output_bytes=" << output.size() << "\n";
    }

    std::cout << "R12_OFFLINE_ALL_APPLIED\n";
    return 0;
}
