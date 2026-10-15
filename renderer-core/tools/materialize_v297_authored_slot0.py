#!/usr/bin/env python3
"""V297 build-time exact authored slot0 nonmetal island admission.

Apply AFTER V296 source materializer; deterministic, idempotent and fail-fast.
This does not patch DarkSoulsRemastered.exe or introduce another addon.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
p=root/"integrated/integrated_addon.cpp"
s=p.read_text("utf-8")
include_from='#include "dsrrl/runtime/v296_oct10_material_atomic_gate.hpp"'
include_to=include_from+'\n#include "dsrrl/runtime/v297_authored_slot0_material.hpp"'

region_from='''    const auto envspec_family =
        hemenvlerp_bound
            ? dsrrl::runtime::pmetal_envspec_receiver_family::hemenvlerp
            : dsrrl::runtime::pmetal_envspec_receiver_family::stable_hemenv;

    const bool exact_pmetal_envspec =
        dsrrl::runtime::exact_pmetal_envspec_candidate(
            material,
            decision);
'''
region_to='''    const auto envspec_family =
        hemenvlerp_bound
            ? dsrrl::runtime::pmetal_envspec_receiver_family::hemenvlerp
            : dsrrl::runtime::pmetal_envspec_receiver_family::stable_hemenv;

    // Only the three independently authored PTDE slot0 material payloads
    // can recover a currently inactive generic-MR route. Full FLVER SHA,
    // actual parser MTD bytes, PTDE EnvSpec slot, shader RX33..35 and
    // downstream stock-t1 sidecar identity remain strict requirements.
    auto effective_envspec_decision=decision;
#if defined(DSRRL_V297_AUTHORED_SLOT0_DIAG)
    const bool v297_authored =
        dsrrl::runtime::v297::promote_exact_slot0_source(
            material,
            decision,
            decision.receiver_id != 0u ? decision.receiver_id : receiver_id,
            effective_envspec_decision);
    if (v297_authored) {
        static std::atomic_bool first_v297_source{false};
        if (!first_v297_source.exchange(true))
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL V297] stage=ptde_slot0_source_authorized"
                " note=not_runtime_atomic_or_pixel_pass");
    } else if (!decision.active &&
               material.route_index == 0u &&
               receiver_id >= 33u && receiver_id <= 35u) {
        // V291 logged only the first 8 bytes of the live MTD SHA. The
        // owner's observed DullLeather/Body raw SHA prefixes do not exist
        // in the vanilla source MTD binder. Record the complete SHA rather
        // than silently relaxing a material identity or reusing P_Metal.
        static std::atomic<std::uint32_t> logged_mask{0u};
        const auto &rows=dsrrl::runtime::v297::k_authored_slot0;
        for (std::uint32_t row=0u; row<3u; ++row) {
            if (material.semantic_name_hash !=
                    dsrrl::operators::material_response::
                        mtd_semantic_hash(rows[row].name) ||
                dsrrl::runtime::v297::raw_sha_equal(
                    material.raw_mtd_sha256,
                    rows[row].dsr_raw_mtd_sha256))
                continue;
            const std::uint32_t bit=1u<<row;
            if ((logged_mask.fetch_or(bit)&bit)!=0u) break;
            char raw_sha[65]{};
            for (std::uint32_t i=0u;i<32u;++i)
                std::snprintf(raw_sha+2u*i,3u,"%02x",
                    static_cast<unsigned>(
                        material.raw_mtd_sha256[i]));
            char line[320]{};
            std::snprintf(line,sizeof(line),
                "[DSRRL V297] stage=raw_mtd_mismatch"
                " mtd_name=%s sha256=%s rx=%u slot=%u"
                " owner=%u parser_exact=%u action=stock_dsr",
                rows[row].name,raw_sha,receiver_id,
                material.material_slot,
                material.owner_tuple_exact?1u:0u,
                material.actual_material_exact?1u:0u);
            reshade::log::message(
                reshade::log::level::warning,line);
            break;
        }
    }
#endif

    const bool exact_pmetal_envspec =
        dsrrl::runtime::exact_pmetal_envspec_candidate(
            material,
            effective_envspec_decision);
'''
prepare_from='''        if (!g_pmetal_envspec.prepare(
                cmd_list,
                material,
                decision,
                envspec_family,
                prepared.envspec)) {'''
prepare_to='''        if (!g_pmetal_envspec.prepare(
                cmd_list,
                material,
                effective_envspec_decision,
                envspec_family,
                prepared.envspec)) {'''

for old,new in [(include_from,include_to),(region_from,region_to),(prepare_from,prepare_to)]:
    if s.count(new)==1:
        continue
    if s.count(old)!=1:
        raise RuntimeError("Source drift or missing V296 materializer: "+old[:48])
    s=s.replace(old,new,1)
assert s.count("effective_envspec_decision")==4,s.count("v297_authored")==2
p.write_text(s,"utf-8")
print("V297_SOURCE_PATCH_PASS slot0_authority=exact_FLVER_SHA_slot_MTD_Rx")
