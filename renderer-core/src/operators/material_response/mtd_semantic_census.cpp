#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/material_response/generated_routes_v1.hpp"
#include "dsrrl/operators/material_response/generated_envspec_router_v1.hpp"

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

struct exact_binding_seed {
    const char *name;
    const char *sha256;
    const char *material_family;
    std::uint32_t receiver0;
    std::uint32_t receiver1;
    std::uint32_t receiver2;
    mtd_gate_policy gate_policy;
};

inline constexpr exact_binding_seed k_exact_bindings[] = {
    {"P_Metal[DSB]_Spec.mtd","c8504bfa64c84d5035bc56ab63e43d319ef69af08c5044e0260b639ab1b78ffc","DifSpcBmp",33u,34u,35u,mtd_gate_policy::direct_exact},
    {"P_DullLeather[DSB]_Edge_Spec.mtd","9d24347d2bfa90df062f39a281fd0ae07f160203f1dc8c5d114ac218a6daa4c2","DifSpcBmp",33u,34u,35u,mtd_gate_policy::direct_exact},
    {"P_DullLeather[DSB]_Spec.mtd","4def97828f95a75b64641fa9f64ba1fc58e909e69479c0d4460aaaf9cd466ee5","DifSpcBmp",33u,34u,35u,mtd_gate_policy::direct_exact},
    {"P_Leather[DSB]_Alp_Spec.mtd","deeb55a8e4d50767aeaba98ba9bfa97b0fb404f901249f5ca13fa762610debda","DifSpcBmp",33u,34u,35u,mtd_gate_policy::direct_exact},
    {"P_Leather[DS].mtd","53819ead337c1ecd8593d8535c1fc1fdde589f00eb8ea015a8ae347da71073ea","DifSpcBmp",33u,34u,35u,mtd_gate_policy::direct_exact},
    {"P_Leather[DSB].mtd","0f2b9a1012b83c4dfc909c311d29a11147de8c15191bc62398ce0d5e1cc6757e","DifSpcBmp",33u,34u,35u,mtd_gate_policy::ptde_companion_required},
    {"C_DullLeather[DSB].mtd","613a3296e7906b2dea5c0cd48bda37c8628b850bad9988688a40595c4c49eaad","DifSpcBmp",33u,34u,35u,mtd_gate_policy::ptde_companion_required},
    {"C_Wet[DSB].mtd","d3885a4c00b844cdb1a09b571cb037343e5bad31249e7ec80ed5e7da195b9e6f","DifSpcBmp",33u,34u,35u,mtd_gate_policy::ptde_companion_required}
};

constexpr std::uint32_t k_exact_binding_use_mask =
    op_bit(mtd_semantic_operator::material_response) |
    op_bit(mtd_semantic_operator::hemenv);

constexpr std::uint32_t k_exact_binding_no_use_mask =
    op_bit(mtd_semantic_operator::pointlight);

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

mtd_envspec_semantics classify_mtd_envspec_semantics(
    const mtd_semantic_query &query) noexcept
{
    mtd_envspec_semantics out;
    if(!query.material.valid || query.material.semantic_name_hash==0u)
        return out;

    for(const auto &seed:generated::k_envspec_router_v1){
        if(query.material.semantic_name_hash!=seed.semantic_name_hash ||
           query.material.raw_mtd_sha256!=seed.raw_mtd_sha256)
            continue;

        out.exact_identity_match=true;
        out.envspc_slot_valid=seed.envspc_slot<4u;
        out.envspc_slot=seed.envspc_slot;
        out.suppress_dsr_only_safe=seed.explicit_none_safe;

        switch(seed.state){
        case generated::envspec_router_state::present:
            out.presence=ptde_envspec_presence::present;
            out.router_state=mtd_envspec_router_state::present;
            break;
        case generated::envspec_router_state::explicit_none:
            out.presence=ptde_envspec_presence::absent;
            out.router_state=mtd_envspec_router_state::explicit_none;
            break;
        case generated::envspec_router_state::nospc_host:
            out.presence=ptde_envspec_presence::absent;
            out.router_state=mtd_envspec_router_state::nospc_host;
            out.suppress_dsr_only_safe=false;
            break;
        case generated::envspec_router_state::unknown:
        default:
            out.presence=ptde_envspec_presence::unknown;
            out.router_state=mtd_envspec_router_state::unknown;
            out.suppress_dsr_only_safe=false;
            break;
        }
        return out;
    }

    return out;
}

mtd_envspec_semantics classify_mtd_envspec_semantics_legacy(
    std::uint64_t legacy_name_hash_utf16_lower,
    const core::sha256_digest &raw_mtd_sha256) noexcept
{
    mtd_envspec_semantics out;
    if(legacy_name_hash_utf16_lower==0u)
        return out;

    for(const auto &seed:generated::k_envspec_router_v1){
        if(seed.legacy_name_hash_utf16_lower!=legacy_name_hash_utf16_lower ||
           seed.raw_mtd_sha256!=raw_mtd_sha256)
            continue;

        out.exact_identity_match=true;
        out.envspc_slot_valid=seed.envspc_slot<4u;
        out.envspc_slot=seed.envspc_slot;
        out.suppress_dsr_only_safe=seed.explicit_none_safe;

        switch(seed.state){
        case generated::envspec_router_state::present:
            out.presence=ptde_envspec_presence::present;
            out.router_state=mtd_envspec_router_state::present;
            break;
        case generated::envspec_router_state::explicit_none:
            out.presence=ptde_envspec_presence::absent;
            out.router_state=mtd_envspec_router_state::explicit_none;
            break;
        case generated::envspec_router_state::nospc_host:
            out.presence=ptde_envspec_presence::absent;
            out.router_state=mtd_envspec_router_state::nospc_host;
            out.suppress_dsr_only_safe=false;
            break;
        case generated::envspec_router_state::unknown:
        default:
            out.presence=ptde_envspec_presence::unknown;
            out.router_state=mtd_envspec_router_state::unknown;
            out.suppress_dsr_only_safe=false;
            break;
        }
        return out;
    }
    return out;
}

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

    if(op==mtd_semantic_operator::env_spec){
        const auto envspec=classify_mtd_envspec_semantics(query);
        if(envspec.exact_identity_match){
            const auto state=
                envspec.presence==ptde_envspec_presence::present
                    ? mtd_semantic_state::use
                    : envspec.presence==ptde_envspec_presence::absent
                        ? mtd_semantic_state::no_use
                        : mtd_semantic_state::unknown;
            return {
                state,
                mtd_semantic_source::envspec_router_exact,
                mtd_gate_policy::exact_material,
                true
            };
        }
    }

    for(const auto &seed:k_overrides){
        if(!receiver_matches(
               query.receiver_id,seed.receiver0,seed.receiver1,seed.receiver2) ||
           !exact_identity(query.material,seed.name,seed.sha256))
            continue;

        const auto state=state_from_masks(op,seed.use_mask,seed.no_use_mask);
        if(state!=mtd_semantic_state::unknown)
            return {
                state,
                mtd_semantic_source::exact_override,
                mtd_gate_policy::exact_material,
                true
            };
        break;
    }

    for(const auto &seed:k_exact_bindings){
        if(!receiver_matches(
               query.receiver_id,seed.receiver0,seed.receiver1,seed.receiver2) ||
           !exact_identity(query.material,seed.name,seed.sha256) ||
           query.material.material_family_hash==0u ||
           query.material.material_family_hash!=
               mtd_semantic_hash(seed.material_family))
            continue;

        return {
            state_from_masks(
                op,
                k_exact_binding_use_mask,
                k_exact_binding_no_use_mask),
            mtd_semantic_source::exact_binding_extension,
            seed.gate_policy,
            true
        };
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
            mtd_gate_policy::exact_material,
            true
        };
    }

    return {};
}

ptde_envspec_presence mtd_envspec_presence(
    const mtd_semantic_query &query) noexcept
{
    const auto exact=classify_mtd_envspec_semantics(query);
    if(exact.exact_identity_match)
        return exact.presence;

    const auto result=classify_mtd_semantic(
        query,mtd_semantic_operator::env_spec);

    if(result.state==mtd_semantic_state::use)
        return ptde_envspec_presence::present;
    if(result.state==mtd_semantic_state::no_use)
        return ptde_envspec_presence::absent;
    return ptde_envspec_presence::unknown;
}

} // namespace dsrrl::operators::material_response
