#pragma once
// V297 DIAGNOSTIC/OPT-IN: original PTDE SPX slot-0 material signal for
// EXACT DSR material payloads, not a generic P_Metal material profile.
// All three rows are independently sourced from the full DSR raw MTD BND3
// and the authored PTDE material donor registry. Runtime pixel OPEN.
// Receiver/material identity, full source FLVER+slot ownership and the
// currently bound Oct10 stock t1 companion must all be re-proven downstream.
#include "dsrrl/runtime/v203_equipment_envspec36_gate.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include <cmath>
#include <cstdint>

namespace dsrrl::runtime::v297 {

namespace mr = dsrrl::operators::material_response;

struct authored_slot0_row {
    const char *name;
    const char *dsr_raw_mtd_sha256;
    float ptde_c100;
    float ptde_c101;
    float ptde_c102;
    unsigned provenance_id;
};

// DSR Mtd.mtdbnd original SHA256; PTDE-authored constants from the
// SHA-identical keys in ptde_material_donor_registry.hpp (source corpus).
inline constexpr authored_slot0_row k_authored_slot0[] = {
    {"P_DullLeather[DSB].mtd",
     "7ea7d6ce9bb7148db62e8c27ee45edefb9547018d0b723dd4753d4c37e29f293",
     0.5f,1.0f,2.0f,460u},
    {"P_DullLeather[DSB]_Edge.mtd",
     "44e1188bafa99d49349883dcc2052c13365f196e341ca5e7c788cbad0a4e058d",
     0.5f,1.0f,2.0f,461u},
    {"Ps_Body[DSB].mtd",
     "af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95",
     0.5f,1.0f,8.0f,492u}
};

inline bool raw_sha_equal(
    const dsrrl::core::sha256_digest &sha,
    const char *hex) noexcept
{
    for (unsigned i=0; i<32u; ++i) {
        auto half=[](char c) noexcept -> int {
            if(c>='0'&&c<='9') return c-'0';
            if(c>='a'&&c<='f') return c-'a'+10;
            return -1;
        };
        const int hi=half(hex[i*2u]);
        const int lo=half(hex[i*2u+1u]);
        if (hi<0 || lo<0 || sha[i] != static_cast<unsigned>((hi<<4)|lo))
            return false;
    }
    return hex[64]=='\0';
}

inline bool promote_exact_slot0_source(
    const mr::material_identity &m,
    const mr::decision &host_decision,
    std::uint32_t receiver,
    mr::decision &out) noexcept
{
    out=host_decision;
    // Only fill an INACTIVE retail route; do not replace an authorized MR
    // decision. No P_Metal/route345 or other light operator can be demoted.
    if (host_decision.active ||
        !m.valid || !m.actual_material_exact ||
        !m.owner_tuple_exact || !m.material_slot_valid ||
        (receiver < 33u || receiver > 35u))
        return false;

    const authored_slot0_row *row=nullptr;
    for (const auto &r:k_authored_slot0) {
        if (m.semantic_name_hash != mr::mtd_semantic_hash(r.name) ||
            !raw_sha_equal(m.raw_mtd_sha256,r.dsr_raw_mtd_sha256))
            continue;
        if (row!=nullptr) return false;
        row=&r;
    }
    if (row==nullptr)
        return false;

    const auto query=
        dsrrl::runtime::v203_equipment::make_envspec36_query(m);
    if (!mr::has_exact_flver_material_ownership(query))
        return false;
    const auto env=mr::classify_mtd_envspec_semantics(query);
    if (!env.exact_identity_match ||
        env.presence != mr::ptde_envspec_presence::present ||
        env.router_state != mr::mtd_envspec_router_state::present ||
        !env.envspc_slot_valid || env.envspc_slot!=0u)
        return false;

    out.active=true;
    out.reason=mr::decision_reason::active;
    out.receiver_id=receiver;
    out.route_index=0x51000000u+row->provenance_id;
    out.certified_operations=
        mr::diffuse_material_domain_linear | mr::specular_factor_c101;
    out.c100={{row->ptde_c100,row->ptde_c100,row->ptde_c100}};
    out.c101=row->ptde_c101;
    out.ptde_specular_power=row->ptde_c102;
    out.ptde_specular_power_verified=true;
    out.envspec=mr::ptde_envspec_presence::present;
    return true;
}

} // namespace dsrrl::runtime::v297
