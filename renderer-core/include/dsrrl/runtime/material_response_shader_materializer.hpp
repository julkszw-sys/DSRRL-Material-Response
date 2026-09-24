#pragma once

#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include "dsrrl/runtime/generated_material_response_v211.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace dsrrl::runtime {

enum class material_response_materialize_result : std::uint8_t {
    applied = 0,
    unknown_shader,
    invalid_dxbc,
    malformed_chunks,
    shex_missing,
    token_mismatch,
    checksum_failed,
    certified_sha_mismatch,
    allocation_failed
};

struct material_response_materialize_outcome {
    material_response_materialize_result result =
        material_response_materialize_result::unknown_shader;
    const material_response_generated::host_recipe *recipe = nullptr;
    bool full_c101 = false;
};

namespace material_response_materializer_detail {

using operators::legacy_plan::dxbc::read_u32;
using operators::legacy_plan::dxbc::write_u32;

inline const material_response_generated::host_recipe *find_recipe(
    const std::uint8_t *source,
    std::size_t size) noexcept
{
    if (source == nullptr || size == 0u)
        return nullptr;

    bool candidate = false;
    for (const auto &recipe : material_response_generated::k_hosts) {
        if (recipe.stock_size == size) {
            candidate = true;
            break;
        }
    }
    if (!candidate)
        return nullptr;

    const auto digest =
        operators::legacy_plan::hashing::sha256(source, size);

    for (const auto &recipe : material_response_generated::k_hosts) {
        if (recipe.stock_size == size &&
            operators::legacy_plan::hashing::matches_hex(
                digest,
                recipe.stock_sha256))
            return &recipe;
    }

    return nullptr;
}

inline bool word_equals(
    const std::vector<std::uint32_t> &words,
    std::size_t index,
    std::uint32_t value) noexcept
{
    return index < words.size() && words[index] == value;
}

inline bool replace_word(
    std::vector<std::uint32_t> &words,
    const material_response_generated::patch_word &patch) noexcept
{
    if (patch.word >= words.size() ||
        words[patch.word] != patch.old_word)
        return false;

    words[patch.word] = patch.new_word;
    return true;
}

inline bool build_shex(
    const std::uint8_t *payload,
    std::size_t payload_size,
    const material_response_generated::host_recipe &recipe,
    bool full_c101,
    std::vector<std::uint8_t> &out)
{
    if (payload == nullptr ||
        payload_size == 0u ||
        (payload_size % sizeof(std::uint32_t)) != 0u)
        return false;

    const std::size_t word_count =
        payload_size / sizeof(std::uint32_t);

    std::vector<std::uint32_t> words(word_count);
    std::memcpy(words.data(), payload, payload_size);

    if (words.size() <= 11u ||
        words[1] != words.size())
        return false;

    if (!word_equals(words, recipe.cb_pair0, 0u) ||
        !word_equals(words, recipe.cb_pair0 + 1u, 9u) ||
        !word_equals(words, recipe.cb_pair1, 0u) ||
        !word_equals(words, recipe.cb_pair1 + 1u, 9u))
        return false;

    for (std::uint32_t i = 0; i < 3u; ++i)
        if (!word_equals(
                words,
                static_cast<std::size_t>(recipe.diffuse_pow) + i,
                0x400ccccdu))
            return false;

    words[recipe.cb_pair0] = 12u;
    words[recipe.cb_pair0 + 1u] = 1u;
    words[recipe.cb_pair1] = 12u;
    words[recipe.cb_pair1 + 1u] = 1u;

    for (std::uint32_t i = 0; i < 3u; ++i)
        words[static_cast<std::size_t>(recipe.diffuse_pow) + i] =
            0x3f800000u;

    words.insert(
        words.begin() + 11,
        material_response_generated::k_cb12_decl.begin(),
        material_response_generated::k_cb12_decl.end());

    words[1] +=
        static_cast<std::uint32_t>(
            material_response_generated::k_cb12_decl.size());

    if (full_c101) {
        for (const auto &patch : recipe.c101)
            if (!replace_word(words, patch))
                return false;

        if (!replace_word(words, recipe.unbounded_mul))
            return false;
    }

    out.resize(words.size() * sizeof(std::uint32_t));
    std::memcpy(out.data(), words.data(), out.size());
    return true;
}

} // namespace material_response_materializer_detail

inline material_response_materialize_outcome
materialize_material_response_shader(
    const std::uint8_t *source,
    std::size_t size,
    bool full_c101,
    std::vector<std::uint8_t> &output) noexcept
{
    using namespace material_response_materializer_detail;

    material_response_materialize_outcome outcome;
    outcome.full_c101 = full_c101;
    output.clear();

    const auto *recipe = find_recipe(source, size);
    if (recipe == nullptr)
        return outcome;

    outcome.recipe = recipe;

    if (!operators::legacy_plan::dxbc::checksum_container_valid(
            source,
            size)) {
        outcome.result =
            material_response_materialize_result::invalid_dxbc;
        return outcome;
    }

    try {
        const std::uint32_t chunk_count = read_u32(source + 28u);
        const std::size_t header_size =
            32u +
            static_cast<std::size_t>(chunk_count) *
                sizeof(std::uint32_t);

        if (chunk_count == 0u ||
            header_size > size) {
            outcome.result =
                material_response_materialize_result::malformed_chunks;
            return outcome;
        }

        struct chunk_record {
            std::uint32_t tag = 0;
            std::vector<std::uint8_t> payload;
        };

        std::vector<chunk_record> chunks;
        chunks.reserve(chunk_count);

        bool found_shex = false;

        for (std::uint32_t i = 0; i < chunk_count; ++i) {
            const std::uint32_t offset =
                read_u32(source + 32u + i * 4u);

            if (offset > size ||
                size - offset < 8u) {
                outcome.result =
                    material_response_materialize_result::malformed_chunks;
                return outcome;
            }

            const std::uint32_t tag = read_u32(source + offset);
            const std::uint32_t payload_size =
                read_u32(source + offset + 4u);

            if (payload_size > size - offset - 8u) {
                outcome.result =
                    material_response_materialize_result::malformed_chunks;
                return outcome;
            }

            chunk_record chunk;
            chunk.tag = tag;

            const auto *payload = source + offset + 8u;

            if (tag == 0x58454853u) { // SHEX
                if (found_shex) {
                    outcome.result =
                        material_response_materialize_result::malformed_chunks;
                    return outcome;
                }

                found_shex = true;
                if (!build_shex(
                        payload,
                        payload_size,
                        *recipe,
                        full_c101,
                        chunk.payload)) {
                    outcome.result =
                        material_response_materialize_result::token_mismatch;
                    return outcome;
                }
            } else {
                chunk.payload.assign(
                    payload,
                    payload + payload_size);
            }

            chunks.emplace_back(std::move(chunk));
        }

        if (!found_shex) {
            outcome.result =
                material_response_materialize_result::shex_missing;
            return outcome;
        }

        output.assign(header_size, 0u);
        std::memcpy(output.data(), source, header_size);

        std::vector<std::uint32_t> offsets;
        offsets.reserve(chunk_count);

        for (const auto &chunk : chunks) {
            offsets.push_back(
                static_cast<std::uint32_t>(output.size()));

            const std::size_t old_size = output.size();
            output.resize(old_size + 8u + chunk.payload.size());

            write_u32(output.data() + old_size, chunk.tag);
            write_u32(
                output.data() + old_size + 4u,
                static_cast<std::uint32_t>(chunk.payload.size()));

            if (!chunk.payload.empty())
                std::memcpy(
                    output.data() + old_size + 8u,
                    chunk.payload.data(),
                    chunk.payload.size());
        }

        write_u32(
            output.data() + 24u,
            static_cast<std::uint32_t>(output.size()));

        for (std::uint32_t i = 0; i < chunk_count; ++i)
            write_u32(
                output.data() + 32u + i * 4u,
                offsets[i]);

        if (!operators::legacy_plan::dxbc::fix_checksum(
                output.data(),
                output.size())) {
            output.clear();
            outcome.result =
                material_response_materialize_result::checksum_failed;
            return outcome;
        }

        const std::size_t expected_size =
            full_c101
                ? recipe->full_size
                : recipe->diffuse_size;
        const auto expected_sha =
            full_c101
                ? recipe->full_sha256
                : recipe->diffuse_sha256;

        if (output.size() != expected_size ||
            !operators::legacy_plan::hashing::matches_hex(
                operators::legacy_plan::hashing::sha256(
                    output.data(),
                    output.size()),
                expected_sha)) {
            output.clear();
            outcome.result =
                material_response_materialize_result::
                    certified_sha_mismatch;
            return outcome;
        }

        outcome.result =
            material_response_materialize_result::applied;
        return outcome;
    }
    catch (...) {
        output.clear();
        outcome.result =
            material_response_materialize_result::allocation_failed;
        return outcome;
    }
}

} // namespace dsrrl::runtime
