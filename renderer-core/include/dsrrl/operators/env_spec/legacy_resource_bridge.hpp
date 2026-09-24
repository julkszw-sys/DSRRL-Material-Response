#pragma once

#include "dsrrl/operators/env_spec/generated_native_probe_hash_v1.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace dsrrl::operators::env_spec {

// Source: recovered Material Response 1.45 V15.7
// V12_ROUTE_ENVSPC_SLOT_MAP_COMPLETE.json
// Historical source SHA-256:
// 046fcbc32781f37475fb9a101b0483bb2fb4b969bf14438201a128c01b45440f
//
// This is routing authority only. It does not authorize activation of the
// legacy EnvSpec island or imply PTDE-visible pixel equivalence.
inline constexpr std::size_t k_legacy_envspec_route_count = 368u;
inline constexpr std::size_t k_legacy_envspec_probe_count = 342u;
inline constexpr std::size_t k_legacy_envspec_endpoint_count = 4u;

inline constexpr std::array<std::uint8_t,k_legacy_envspec_route_count>
k_legacy_envspec_slot_by_route = {{
    1u, 1u, 2u, 0u, 2u, 2u, 0u, 0u, 3u, 3u, 0u, 0u, 1u, 2u, 0u, 0u, 3u, 3u, 2u, 0u, 0u, 1u, 1u, 1u,
    0u, 3u, 0u, 1u, 0u, 0u, 0u, 0u, 1u, 3u, 3u, 2u, 2u, 0u, 0u, 1u, 0u, 1u, 0u, 0u, 1u, 2u, 1u, 0u,
    0u, 0u, 3u, 1u, 0u, 0u, 3u, 2u, 0u, 3u, 0u, 1u, 0u, 0u, 1u, 2u, 0u, 0u, 0u, 3u, 0u, 0u, 0u, 0u,
    1u, 1u, 2u, 0u, 2u, 0u, 0u, 0u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 2u, 0u, 2u, 2u, 2u, 3u, 0u, 0u, 0u,
    3u, 0u, 1u, 3u, 1u, 0u, 2u, 0u, 1u, 3u, 2u, 1u, 0u, 0u, 1u, 0u, 0u, 3u, 3u, 0u, 3u, 3u, 1u, 0u,
    2u, 0u, 0u, 1u, 1u, 0u, 0u, 3u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, 3u, 0u, 2u, 0u, 2u, 0u, 2u, 2u, 0u,
    0u, 3u, 1u, 0u, 2u, 3u, 3u, 3u, 0u, 0u, 0u, 3u, 0u, 3u, 0u, 1u, 0u, 0u, 0u, 2u, 1u, 0u, 0u, 3u,
    1u, 2u, 0u, 0u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 0u, 3u, 0u, 1u, 0u, 0u, 0u, 3u, 0u, 3u, 0u,
    0u, 0u, 0u, 2u, 0u, 3u, 0u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 3u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 2u,
    0u, 0u, 0u, 0u, 0u, 0u, 3u, 0u, 0u, 0u, 0u, 0u, 3u, 2u, 0u, 3u, 0u, 0u, 0u, 0u, 1u, 0u, 3u, 0u,
    2u, 3u, 0u, 1u, 3u, 0u, 0u, 0u, 0u, 2u, 0u, 2u, 1u, 0u, 3u, 2u, 1u, 0u, 0u, 0u, 1u, 0u, 2u, 0u,
    1u, 2u, 0u, 3u, 3u, 1u, 1u, 0u, 1u, 2u, 0u, 0u, 0u, 0u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 2u,
    3u, 2u, 0u, 2u, 2u, 1u, 2u, 1u, 0u, 0u, 2u, 2u, 1u, 2u, 3u, 2u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 2u,
    0u, 3u, 2u, 2u, 1u, 0u, 0u, 2u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 3u, 2u, 0u, 2u, 0u, 0u, 1u, 0u, 1u,
    0u, 1u, 1u, 0u, 0u, 0u, 3u, 3u, 0u, 2u, 2u, 3u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 1u, 0u, 3u, 3u, 2u,
    0u, 1u, 0u, 0u, 2u, 0u, 3u, 0u
}};

constexpr bool legacy_envspec_slot_map_valid() noexcept
{
    for(const auto slot:k_legacy_envspec_slot_by_route)
        if(slot>=k_legacy_envspec_endpoint_count)
            return false;
    return true;
}

static_assert(legacy_envspec_slot_map_valid());

inline std::optional<std::uint8_t> legacy_envspec_slot_for_route(
    std::uint32_t route_index) noexcept
{
    if(route_index>=k_legacy_envspec_slot_by_route.size())
        return std::nullopt;
    return k_legacy_envspec_slot_by_route[route_index];
}

static_assert(
    generated::k_native_probe_hash_record_count==k_legacy_envspec_probe_count,
    "Native EnvSpec fingerprint corpus must cover all 342 canonical probes.");

inline std::optional<std::uint16_t> legacy_native_probe_for_sha(
    const std::array<std::uint8_t,32> &sha256) noexcept
{
    for(const auto &record:generated::k_native_probe_hash_v1)
        if(record.sha256==sha256)
            return record.probe_ordinal;
    return std::nullopt;
}

// Exact external resource contract recovered from the 1.45 PackedGI loader.
// A modern implementation may load this data through a different mechanism,
// but size/hash failure must remain fail-open.
inline constexpr char k_legacy_packed_gi_relative_path[] =
    "DSRRL\\EnvSpec\\PackedGI\\PTDE_GI_ENVSPEC_PACK_RGBA.bin";
inline constexpr std::uint64_t k_legacy_packed_gi_size = 33619968ull;
inline constexpr std::array<std::uint8_t,32> k_legacy_packed_gi_sha256 = {{
    0xc1u,0x6cu,0x3fu,0xd7u,0x5bu,0xcfu,0x34u,0xf3u,
    0xccu,0x07u,0x5du,0xa6u,0xdau,0x1au,0xd1u,0x0cu,
    0x94u,0x40u,0xeeu,0x4au,0x3cu,0xa5u,0x80u,0xfeu,
    0x7fu,0x74u,0xd0u,0x7au,0x2cu,0xe4u,0xeau,0xc3u
}};

enum class legacy_envspec_identity_observe_result : std::uint8_t {
    invalid = 0,
    learning,
    refreshing,
    established
};

enum class legacy_envspec_identity_resolution_state : std::uint8_t {
    not_found = 0,
    unique,
    ambiguous
};

struct legacy_envspec_identity_observation {
    std::uint32_t canonical_probe_ordinal = 0;
    std::uint32_t semantic_a = 0;
    std::uint32_t semantic_b = 0;
    std::uint64_t stock_t12 = 0;
    std::uint64_t stock_t14 = 0;
    bool fresh = false;
};

struct legacy_envspec_identity_resolution {
    legacy_envspec_identity_resolution_state state =
        legacy_envspec_identity_resolution_state::not_found;
    std::uint32_t canonical_probe_ordinal = 0;
};

// Source-complete reconstruction of the verified V15.1/V15.7 identity policy:
// two identical fresh observations are required before an identity becomes
// usable; a changed observation restarts learning; A==B is legal only when
// stock t12==t14; resolution must be unique. Learning/refreshing/ambiguous
// states are intentionally fail-open.
class legacy_envspec_identity_tracker {
public:
    legacy_envspec_identity_observe_result observe(
        const legacy_envspec_identity_observation &observation) noexcept
    {
        if(!observation.fresh ||
           observation.canonical_probe_ordinal>=records_.size() ||
           observation.stock_t12==0u ||
           observation.stock_t14==0u ||
           (observation.semantic_a==observation.semantic_b &&
            observation.stock_t12!=observation.stock_t14))
            return legacy_envspec_identity_observe_result::invalid;

        auto &record=records_[observation.canonical_probe_ordinal];
        const bool same=
            record.observations!=0u &&
            record.semantic_a==observation.semantic_a &&
            record.semantic_b==observation.semantic_b &&
            record.stock_t12==observation.stock_t12 &&
            record.stock_t14==observation.stock_t14;

        if(!same){
            const bool replacing=record.observations!=0u;
            record.semantic_a=observation.semantic_a;
            record.semantic_b=observation.semantic_b;
            record.stock_t12=observation.stock_t12;
            record.stock_t14=observation.stock_t14;
            record.observations=1u;
            record.established=false;
            return replacing
                ? legacy_envspec_identity_observe_result::refreshing
                : legacy_envspec_identity_observe_result::learning;
        }

        if(!record.established){
            if(record.observations<2u)
                ++record.observations;
            if(record.observations>=2u)
                record.established=true;
        }

        return record.established
            ? legacy_envspec_identity_observe_result::established
            : legacy_envspec_identity_observe_result::learning;
    }

    legacy_envspec_identity_resolution resolve(
        std::uint64_t stock_t12,
        std::uint64_t stock_t14) const noexcept
    {
        legacy_envspec_identity_resolution out;
        if(stock_t12==0u||stock_t14==0u)
            return out;

        bool found=false;
        for(std::uint32_t i=0;i<records_.size();++i){
            const auto &record=records_[i];
            if(!record.established ||
               record.stock_t12!=stock_t12 ||
               record.stock_t14!=stock_t14)
                continue;

            if(found){
                out.state=
                    legacy_envspec_identity_resolution_state::ambiguous;
                out.canonical_probe_ordinal=0u;
                return out;
            }

            found=true;
            out.state=legacy_envspec_identity_resolution_state::unique;
            out.canonical_probe_ordinal=i;
        }
        return out;
    }

    void reset() noexcept
    {
        records_={};
    }

private:
    struct identity_record {
        std::uint32_t semantic_a = 0;
        std::uint32_t semantic_b = 0;
        std::uint64_t stock_t12 = 0;
        std::uint64_t stock_t14 = 0;
        std::uint8_t observations = 0;
        bool established = false;
    };

    std::array<identity_record,k_legacy_envspec_probe_count> records_{};
};

} // namespace dsrrl::operators::env_spec
