#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/material_response/generated_routes_v1.hpp"

#include <cstddef>

namespace dsrrl::operators::material_response {
namespace {

constexpr std::uint32_t op_bit(mtd_semantic_operator op) noexcept
{
    return 1u << static_cast<std::uint8_t>(op);
}

constexpr std::uint32_t k_full24_use_mask =
    op_bit(mtd_semantic_operator::material_response) |
    op_bit(mtd_semantic_operator::spec_rgb) |
    op_bit(mtd_semantic_operator::hemenv) |
    op_bit(mtd_semantic_operator::texture_resource_consumers);

constexpr std::uint32_t k_full24_no_use_mask =
    op_bit(mtd_semantic_operator::pointlight);

struct override_seed {
    const char *name;
    const char *sha256;
    std::uint32_t receiver0;
    std::uint32_t receiver1;
    std::uint32_t receiver2;
    std::uint32_t use_mask;
    std::uint32_t no_use_mask;
};

inline constexpr override_seed k_overrides[] = {
    {
        "Ps_Body[DSB].mtd",
        "af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95",
        33u,34u,35u,0u,op_bit(mtd_semantic_operator::subsurface)
    },
    {
        "P_Metal[DSB].mtd",
        "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b",
        33u,34u,35u,op_bit(mtd_semantic_operator::env_spec),0u
    }
};

int hex_value(char c) noexcept
{
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return 10+c-'a';
    if(c>='A'&&c<='F') return 10+c-'A';
    return -1;
}

bool digest_matches_hex(const core::sha256_digest &digest,const char *hex) noexcept
{
    if(!hex) return false;
    for(std::size_t i=0;i<digest.size();++i){
        const int hi=hex_value(hex[i*2]);
        const int lo=hex_value(hex[i*2+1]);
        if(hi<0||lo<0) return false;
        if(digest[i]!=static_cast<std::uint8_t>((hi<<4)|lo))
            return false;
    }
    return hex[64]=='\0';
}

bool receiver_matches(std::uint32_t r,std::uint32_t a,std::uint32_t b,std::uint32_t c) noexcept
{
    return r==a||r==b||r==c;
}

mtd_semantic_state state_from_masks(
    mtd_semantic_operator op,
    std::uint32_t use_mask,
    std::uint32_t no_use_mask) noexcept
{
    const auto bit=op_bit(op);
    if((use_mask&bit)!=0u) return mtd_semantic_state::use;
    if((no_use_mask&bit)!=0u) return mtd_semantic_state::no_use;
    return mtd_semantic_state::unknown;
}

bool exact_identity(
    const material_identity &identity,
    const char *name,
    const char *sha256) noexcept
{
    return identity.valid &&
           identity.semantic_name_hash!=0u &&
           identity.semantic_name_hash==mtd_semantic_hash(name) &&
           digest_matches_hex(identity.raw_mtd_sha256,sha256);
}

} // namespace

std::uint64_t mtd_semantic_hash(const char *text) noexcept
{
    constexpr std::uint64_t offset=14695981039346656037ull;
    constexpr std::uint64_t prime=1099511628211ull;
    if(!text) return 0u;
    std::uint64_t hash=offset;
    for(;*text!='\0';++text){
        hash^=static_cast<std::uint8_t>(*text);
        hash*=prime;
    }
    return hash;
}

mtd_semantic_decision classify_mtd_semantic(
    const mtd_semantic_query &query,
    mtd_semantic_operator op) noexcept
{
    if(!query.material.valid||query.receiver_id==0u)
        return {};

    for(const auto &seed:k_overrides){
        if(!receiver_matches(
               query.receiver_id,seed.receiver0,seed.receiver1,seed.receiver2) ||
           !exact_identity(query.material,seed.name,seed.sha256))
            continue;

        const auto state=state_from_masks(op,seed.use_mask,seed.no_use_mask);
        if(state!=mtd_semantic_state::unknown)
            return {state,mtd_semantic_source::exact_override,true};
        break;
    }

    for(const auto &seed:generated::k_material_routes_v1){
        if(query.material.route_index!=seed.route_index ||
           query.material.semantic_name_hash==0u ||
           query.material.semantic_name_hash!=mtd_semantic_hash(seed.mtd_name) ||
           !digest_matches_hex(query.material.raw_mtd_sha256,seed.sha256) ||
           query.material.material_family_hash==0u ||
           query.material.material_family_hash!=mtd_semantic_hash(seed.material_family) ||
           !receiver_matches(
               query.receiver_id,seed.receiver0,seed.receiver1,seed.receiver2))
            continue;

        return {
            state_from_masks(op,k_full24_use_mask,k_full24_no_use_mask),
            mtd_semantic_source::full24_exact_cohort,
            true
        };
    }

    return {};
}

ptde_envspec_presence mtd_envspec_presence(
    const mtd_semantic_query &query) noexcept
{
    const auto result=classify_mtd_semantic(
        query,mtd_semantic_operator::env_spec);

    if(result.state==mtd_semantic_state::use)
        return ptde_envspec_presence::present;
    if(result.state==mtd_semantic_state::no_use)
        return ptde_envspec_presence::absent;
    return ptde_envspec_presence::unknown;
}

} // namespace dsrrl::operators::material_response
