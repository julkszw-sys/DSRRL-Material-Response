#pragma once
// V311: Exact direct-FLVER-parser owner recovery from independently live
// MTD parser bytes; NO texture-name, SPX-family, pointer-only or MTD-only
// owner inference. Candidate must already belong to a PTDE-authored,
// operator/receiver-certified material cohort (V297 or V300).
#include "dsrrl/runtime/v298_parser_mtd_witness.hpp"
#include "dsrrl/runtime/v300_exact_leather_material.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include <algorithm>
#include <cstdint>

namespace dsrrl::runtime::v311 {
namespace mr=dsrrl::operators::material_response;
using exact_owner_lookup=bool(*)(
    const core::sha256_digest&,std::uint32_t,std::uint64_t&) noexcept;

inline bool recover_exact_owner_from_live_parsers(
    const core::sha256_digest &model_sha,
    std::uint32_t slot,
    const void *selected_material,
    const v298::parser_mtd_entry &live,
    exact_owner_lookup owner_mtd_hash,
    mr::material_identity &out) noexcept
{
    out={};
    if(selected_material==nullptr || live.material!=selected_material ||
       live.raw_bytes==0u || owner_mtd_hash==nullptr ||
       std::all_of(model_sha.begin(),model_sha.end(),
                   [](std::uint8_t v){return v==0u;}))
        return false;
    std::uint64_t source_semantic=0u;
    // Original DSR FLVER SHA+slot -> material basename relationship:
    // zero ambiguity and no positive matching from logical t1 alone.
    if(!owner_mtd_hash(model_sha,slot,source_semantic) ||
       source_semantic==0u)
        return false;

    int matched=0;
    std::uint8_t exact_env_slot=255u;
    for(const auto &row:v297::k_authored_slot0) {
        if(source_semantic==mr::mtd_semantic_hash(row.name) &&
           v297::raw_sha_equal(live.raw,row.dsr_raw_mtd_sha256)) {
            ++matched;
            exact_env_slot=0u;
        }
    }
    for(const auto &row:v300::k_leather) {
        if(source_semantic==mr::mtd_semantic_hash(row.name) &&
           v297::raw_sha_equal(live.raw,row.sha)) {
            ++matched;
            exact_env_slot=1u;
        }
    }
    if(matched!=1 || exact_env_slot>1u)
        return false;

    mr::material_identity candidate{};
    candidate.valid=true;
    candidate.flver_sha256=model_sha;
    candidate.material_slot=slot;
    candidate.material_slot_valid=true;
    candidate.owner_tuple_exact=true;
    candidate.actual_material_exact=true;
    candidate.semantic_name_hash=source_semantic;
    candidate.raw_mtd_sha256=live.raw;
    candidate.material_family_hash=mr::mtd_semantic_hash("DifSpcBmp");
    candidate.route_index=0u; // Host unresolved; original PTDE island decides.
    mr::mtd_semantic_query q{};
    q.material=candidate;
    q.ownership.flver_sha256=model_sha;
    q.ownership.material_slot=slot;
    q.ownership.material_slot_valid=true;
    q.ownership.exact=true;
    if(!mr::has_exact_flver_material_ownership(q))
        return false;
    const auto env=mr::classify_mtd_envspec_semantics(q);
    if(!env.exact_identity_match ||
       env.presence!=mr::ptde_envspec_presence::present ||
       env.router_state!=mr::mtd_envspec_router_state::present ||
       !env.envspc_slot_valid || env.envspc_slot!=exact_env_slot)
        return false;
    out=candidate;
    return true;
}
} // namespace dsrrl::runtime::v311
