#include "dsrrl/operators/material_response/material_response_seed.hpp"
#include "dsrrl/operators/material_response/generated_routes_v1.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"

#include <cstdint>

namespace dsrrl::operators::material_response {
namespace {

int hex_value(char c) noexcept
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool parse_sha256(const char *hex, core::sha256_digest &out) noexcept
{
    if (hex == nullptr)
        return false;

    for (std::size_t i = 0; i < out.size(); ++i) {
        const int hi = hex_value(hex[i * 2]);
        const int lo = hex_value(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }

    return hex[64] == '\0';
}

} // namespace

std::size_t register_confirmed_material_routes_v1(material_response_island &island)
{
    std::size_t registered = 0;

    for (const auto &seed : generated::k_material_routes_v1) {
        material_profile profile;
        profile.route_index = seed.route_index;
        profile.semantic_name_hash = mtd_semantic_hash(seed.mtd_name);
        profile.material_family_hash = mtd_semantic_hash(seed.material_family);
        profile.c101 = seed.c101;
        profile.lod_min = seed.lod_min;
        profile.lod_max = seed.lod_max;
        profile.receiver_ids = {
            seed.receiver0,
            seed.receiver1,
            seed.receiver2,
            0u
        };
        profile.receiver_count = 3;
        profile.certified_operations = specular_factor_c101;
        profile.envspec = ptde_envspec_presence::unknown;

        // Conservative exact identity on every seeded route. This is required
        // for the known route5 raw-MTD collision and avoids expanding SHA-only
        // classification to routes that have not been proven collision-free
        // under all future material corpora.
        profile.semantic_name_required = true;

        if (!parse_sha256(seed.sha256, profile.raw_mtd_sha256))
            continue;

        material_identity identity;
        identity.valid = true;
        // Seed validation is MTD/profile construction only. It intentionally
        // carries no FLVER owner provenance; runtime ownership is draw-specific.
        identity.route_index = seed.route_index;
        identity.semantic_name_hash = profile.semantic_name_hash;
        identity.raw_mtd_sha256 = profile.raw_mtd_sha256;
        identity.material_family_hash = profile.material_family_hash;
        const mtd_semantic_query query{identity, seed.receiver0};

        if (classify_mtd_semantic(
                query,
                mtd_semantic_operator::material_response).state !=
            mtd_semantic_state::use)
            continue;

        profile.envspec = mtd_envspec_presence(query);

        if (island.register_material_profile(profile))
            ++registered;
    }

    return registered;
}

} // namespace dsrrl::operators::material_response
