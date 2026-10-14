#pragma once
// V287 one-addon HemEnvLerp migration authority.
// NEW descendants NEVER inherit an unverified R19 shader as fallback.
// Empty approval list is intentional: Lerp uses stock DSR until FINAL V286
// DXBC, exact MTD/slot/SRV ownership and beta endpoints are certified.
// A build may add only SHA+size-authenticated per-receiver final V286 records.
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime::hemenvlerp_v287 {
using sha256_digest =
    operators::legacy_plan::hashing::sha256_digest;

struct certified_receiver {
    std::uint32_t receiver_id;
    std::size_t dxbc_size;
    const char* dxbc_sha256;
};

// No guessed hashes and no admission of old R19.
// Populate only with SHA of real owner-produced final V286 output after
// D3D11/WARP acceptance and protected material/resource verification.
inline constexpr std::array<certified_receiver, 0> approved_final_v286{};

inline bool permits(
    std::uint32_t receiver_id,
    const sha256_digest &sha,
    std::size_t size) noexcept
{
    if (receiver_id < 33u || receiver_id > 35u || size == 0u)
        return false;

    for (const auto &entry : approved_final_v286) {
        if (entry.receiver_id != receiver_id ||
            entry.dxbc_size != size ||
            entry.dxbc_sha256 == nullptr)
            continue;

        if (operators::legacy_plan::hashing::matches_hex(
                sha, entry.dxbc_sha256))
            return true;
    }
    return false;
}

} // namespace dsrrl::runtime::hemenvlerp_v287
