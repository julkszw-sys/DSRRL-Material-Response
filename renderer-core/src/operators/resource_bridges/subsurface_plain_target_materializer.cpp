#include "dsrrl/operators/resource_bridges/subsurface_plain_target_materializer.hpp"

#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include "dsrrl/operators/resource_bridges/generated_subsurface_plain_patches_v1.hpp"

#include <cstring>
#include <limits>

namespace dsrrl::operators::resource_bridges {
namespace {

namespace hashing = legacy_plan::hashing;

std::uint16_t read_u16(
    const std::uint8_t *p) noexcept
{
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(p[0]) |
        (static_cast<std::uint16_t>(p[1]) << 8u));
}

std::uint32_t read_u32(
    const std::uint8_t *p) noexcept
{
    return
        static_cast<std::uint32_t>(p[0]) |
        (static_cast<std::uint32_t>(p[1]) << 8u) |
        (static_cast<std::uint32_t>(p[2]) << 16u) |
        (static_cast<std::uint32_t>(p[3]) << 24u);
}

} // namespace

subsurface_plain_target_materialize_outcome
materialize_subsurface_plain_target(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &target) noexcept
{
    target.clear();

    subsurface_plain_target_materialize_outcome outcome{};

    if (source == nullptr ||
        size == 0u ||
        (size % sizeof(std::uint32_t)) != 0u) {
        outcome.result =
            subsurface_plain_target_materialize_result::
                fail_invalid_source;
        return outcome;
    }

    const auto source_digest =
        hashing::sha256(source, size);

    const generated::subsurface_plain_patch_record *record =
        nullptr;
    for (const auto &candidate :
         generated::k_subsurface_plain_patches) {
        if (candidate.source_size != size)
            continue;
        if (!hashing::matches_hex(
                source_digest,
                candidate.source_sha256))
            continue;
        record = &candidate;
        break;
    }

    if (record == nullptr)
        return outcome;

    outcome.target_plain_receiver_id =
        record->target_receiver_id;

    if (record->patch == nullptr ||
        record->patch_size < 12u ||
        record->target_size == 0u ||
        (record->target_size %
            sizeof(std::uint32_t)) != 0u) {
        outcome.result =
            subsurface_plain_target_materialize_result::
                fail_patch_payload;
        return outcome;
    }

    const auto *patch = record->patch;
    const auto op_count =
        read_u16(patch + 0u);
    const auto literal_words =
        read_u16(patch + 2u);
    const auto source_words =
        read_u32(patch + 4u);
    const auto target_words =
        read_u32(patch + 8u);

    const std::size_t op_bytes =
        static_cast<std::size_t>(op_count) * 5u;
    const std::size_t literal_bytes =
        static_cast<std::size_t>(literal_words) *
        sizeof(std::uint32_t);

    if (source_words != size / sizeof(std::uint32_t) ||
        target_words !=
            record->target_size /
                sizeof(std::uint32_t) ||
        op_bytes >
            std::numeric_limits<std::size_t>::max() -
                12u ||
        12u + op_bytes >
            record->patch_size ||
        literal_bytes >
            record->patch_size -
                (12u + op_bytes) ||
        12u + op_bytes + literal_bytes !=
            record->patch_size) {
        outcome.result =
            subsurface_plain_target_materialize_result::
                fail_patch_payload;
        return outcome;
    }

    try {
        target.resize(record->target_size);
    } catch (...) {
        target.clear();
        outcome.result =
            subsurface_plain_target_materialize_result::
                fail_patch_payload;
        return outcome;
    }

    const auto literal_base =
        12u + op_bytes;
    std::size_t write_words = 0u;

    for (std::uint16_t i = 0u;
         i < op_count;
         ++i) {
        const auto at =
            12u +
            static_cast<std::size_t>(i) * 5u;

        const auto kind = patch[at];
        const auto source_or_literal =
            static_cast<std::size_t>(
                read_u16(patch + at + 1u));
        const auto count =
            static_cast<std::size_t>(
                read_u16(patch + at + 3u));

        if (count == 0u ||
            write_words >
                static_cast<std::size_t>(target_words) ||
            count >
                static_cast<std::size_t>(target_words) -
                    write_words) {
            target.clear();
            outcome.result =
                subsurface_plain_target_materialize_result::
                    fail_patch_payload;
            return outcome;
        }

        const std::uint8_t *copy_source = nullptr;

        if (kind == 0u) {
            if (source_or_literal >
                    static_cast<std::size_t>(
                        source_words) ||
                count >
                    static_cast<std::size_t>(
                        source_words) -
                        source_or_literal) {
                target.clear();
                outcome.result =
                    subsurface_plain_target_materialize_result::
                        fail_patch_payload;
                return outcome;
            }

            copy_source =
                source +
                source_or_literal *
                    sizeof(std::uint32_t);
        } else if (kind == 1u) {
            if (source_or_literal >
                    static_cast<std::size_t>(
                        literal_words) ||
                count >
                    static_cast<std::size_t>(
                        literal_words) -
                        source_or_literal) {
                target.clear();
                outcome.result =
                    subsurface_plain_target_materialize_result::
                        fail_patch_payload;
                return outcome;
            }

            copy_source =
                patch +
                literal_base +
                source_or_literal *
                    sizeof(std::uint32_t);
        } else {
            target.clear();
            outcome.result =
                subsurface_plain_target_materialize_result::
                    fail_patch_payload;
            return outcome;
        }

        std::memcpy(
            target.data() +
                write_words *
                    sizeof(std::uint32_t),
            copy_source,
            count *
                sizeof(std::uint32_t));

        write_words += count;
    }

    if (write_words !=
        static_cast<std::size_t>(target_words)) {
        target.clear();
        outcome.result =
            subsurface_plain_target_materialize_result::
                fail_patch_payload;
        return outcome;
    }

    const auto target_digest =
        hashing::sha256(
            target.data(),
            target.size());
    if (!hashing::matches_hex(
            target_digest,
            record->target_sha256)) {
        target.clear();
        outcome.result =
            subsurface_plain_target_materialize_result::
                fail_target_identity;
        return outcome;
    }

    outcome.result =
        subsurface_plain_target_materialize_result::
            applied;
    return outcome;
}

} // namespace dsrrl::operators::resource_bridges
