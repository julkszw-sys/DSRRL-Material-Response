#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::core {

enum class operator_id : std::uint8_t {
    material_response = 0,
    upper_lower,
    hemdir3,
    spec_rgb,
    env_spec,
    envspec_nospc_delete,
    envspec_pmetal_diagnostic,
    env_diffuse,
    point_light,
    pointlight_pnts_attenuation,
    local_specular_legacy,
    subsurface,
    diffuse,
    normal,
    diffuse_material_domain,
    terminal_sat_rgb,
    terminal_sat_rgba,
    fixed_postfog_identity,
    faceeye_shadow_legacy,
    post_bloom,
    post_hdr,
    dsr_native_sfx,
    dsr_sfx_inverse_tonemap,
    pmetal_black_safe_source,
    count
};

constexpr std::size_t operator_count = static_cast<std::size_t>(operator_id::count);
static_assert(operator_count <= 32, "operator_mask is a 32-bit ABI.");

enum class context_kind : std::uint8_t {
    unknown = 0,
    immediate,
    deferred
};

enum class island_state : std::uint8_t {
    disabled = 0,
    armed,
    active,
    fail_open
};

using operator_mask = std::uint32_t;
using sha256_digest = std::array<std::uint8_t, 32>;

constexpr bool valid_operator_id(operator_id op) noexcept
{
    return static_cast<std::size_t>(op) < operator_count;
}

constexpr operator_mask operator_bit(operator_id op) noexcept
{
    const auto index = static_cast<std::uint32_t>(op);
    return index < operator_count ? (1u << index) : 0u;
}

inline constexpr operator_mask all_operator_bits =
    ~operator_mask{0} >> (32u - static_cast<unsigned>(operator_count));

struct alignas(16) float4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

static_assert(sizeof(float4) == 16, "DSRRL float4 ABI must remain 16 bytes.");

struct semantic_key {
    operator_id op = operator_id::material_response;
    std::uint64_t owner = 0;
    std::uint32_t selector = 0;

    bool operator==(const semantic_key &other) const noexcept
    {
        return op == other.op && owner == other.owner && selector == other.selector;
    }
};

struct semantic_key_hash {
    std::size_t operator()(const semantic_key &key) const noexcept
    {
        std::uint64_t h = key.owner;
        h ^= static_cast<std::uint64_t>(key.selector) << 32;
        h ^= static_cast<std::uint64_t>(key.op) * 0x9E3779B185EBCA87ull;
        return static_cast<std::size_t>(h ^ (h >> 32));
    }
};

} // namespace dsrrl::core
