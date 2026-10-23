#pragma once
// V310: repair a proved stale OFFLINE owner-to-MTD-SHA join using only the
// independently captured ORIGINAL LIVE RETAIL PARSER material bytes.
// This is NOT model ownership reconstruction or a wildcard authorization:
// exact original FLVER SHA+slot+semantic is verified separately before
// replacing the stale generic MTD digest for an authored slot0 operator.
#include "dsrrl/runtime/v297_authored_slot0_material.hpp"
#include "dsrrl/runtime/v298_parser_mtd_witness.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"

namespace dsrrl::runtime::v310 {
namespace mr=dsrrl::operators::material_response;

inline bool reconcile_exact_slot0_owner_parser(
    mr::material_identity &identity,
    const v298::parser_mtd_entry &parser) noexcept
{
    // Never fix a missing owner, a pointer-independent parser observation,
    // an already exact identity, or an existing MR/P_Metal positive route.
    if (!identity.valid || !identity.owner_tuple_exact ||
        !identity.material_slot_valid ||
        identity.actual_material_exact ||
        identity.route_index!=0u ||
        identity.semantic_name_hash==0u ||
        parser.material==nullptr || parser.raw_bytes==0u ||
        identity.raw_mtd_sha256==parser.raw)
        return false;

    // The exact PTDE donor must be unambiguous at a full 256-bit digest
    // level and match the name from the ORIGINAL exact DSR FLVER owner.
    const v297::authored_slot0_row *match=nullptr;
    for(const auto &row:v297::k_authored_slot0) {
        if (identity.semantic_name_hash!=mr::mtd_semantic_hash(row.name) ||
            !v297::raw_sha_equal(parser.raw,row.dsr_raw_mtd_sha256))
            continue;
        if(match!=nullptr) return false;
        match=&row;
    }
    if(match==nullptr) return false;

    mr::mtd_semantic_query query{};
    query.material=identity;
    query.ownership.flver_sha256=identity.flver_sha256;
    query.ownership.flver_identity_hash=identity.flver_identity_hash;
    query.ownership.material_slot=identity.material_slot;
    query.ownership.material_slot_valid=identity.material_slot_valid;
    query.ownership.exact=identity.owner_tuple_exact;
    if(!mr::has_exact_flver_material_ownership(query))
        return false;

    // This is not a compensation gain and not a guessed SHA alias:
    // use exact bytes from the CURRENT MTD parser observation, then prove
    // the original PTDE EnvSpec operator's source row for this MTD.
    auto proposed=identity;
    proposed.raw_mtd_sha256=parser.raw;
    proposed.material_family_hash=mr::mtd_semantic_hash("DifSpcBmp");
    proposed.actual_material_exact=true;
    query.material=proposed;
    const auto env=mr::classify_mtd_envspec_semantics(query);
    if(!env.exact_identity_match ||
       env.presence!=mr::ptde_envspec_presence::present ||
       env.router_state!=mr::mtd_envspec_router_state::present ||
       !env.envspc_slot_valid || env.envspc_slot!=0u)
        return false;

    identity=proposed;
    return true;
}
} // namespace dsrrl::runtime::v310
