#!/usr/bin/env python3
"""V250: bounded P_Metal exact RX33/RX34 LERP prepare-reject reason in release.
Only replaces release-cleanup diagnostics no-op; not operator equation,
material routing, R19 shader, state transaction or any release mode switch.
"""
from pathlib import Path
import argparse
PATH=Path("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
OLD="""#if defined(DSRRL_RELEASE_CLEANUP)
    (void)bit;
    (void)stage;
    (void)material;
    (void)decision;
    (void)family;
#else"""
NEW="""#if defined(DSRRL_RELEASE_CLEANUP)
    // V250: preserve full release cleanup except for the verified P_Metal
    // LERP receiver draw that the owner observed as stock DSR. One record
    // per actual prepare rejection stage; no effect on gate outcome.
    if ((decision.receiver_id == 33u || decision.receiver_id == 34u) &&
        decision.route_index == 345u &&
        material.valid && material.owner_tuple_exact &&
        family == pmetal_envspec_receiver_family::hemenvlerp) {
        static std::atomic<std::uint32_t> seen[2]{};
        const auto receiver_index =
            decision.receiver_id == 33u ? 0u : 1u;
        if ((seen[receiver_index].fetch_or(
                bit, std::memory_order_relaxed) & bit) == 0u) {
            char line[480]{};
            std::snprintf(
                line, sizeof(line),
                "[DSRRL V250 PMETAL LERP PREPARE] rx=%u route=%u "
                "slot=%u stage=%s bit=%u sem=%016llx "
                "owner=%u receiver_family=LERP pixel=OPEN",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(decision.route_index),
                static_cast<unsigned>(material.material_slot),
                stage,
                static_cast<unsigned>(bit),
                static_cast<unsigned long long>(
                    material.semantic_name_hash),
                material.owner_tuple_exact ? 1u : 0u);
            reshade::log::message(
                reshade::log::level::info, line);
        }
    }
#else"""
def patch(src:str)->str:
    if src.count(NEW)==1:return src
    if src.count(OLD)!=1:raise RuntimeError("V250 release source preimage changed")
    if src.count("void log_prepare_stage_once(")!=1:
        raise RuntimeError("V250 logger target changed")
    updated=src.replace(OLD,NEW,1)
    for marker in ("log_prepare_stage_once(", "source_reject",
                   "probe_reject", "spec_rgb_reject",
                   "mutation_reject", "request_ready"):
        if updated.count(marker)!=src.count(marker):
            raise RuntimeError("V250 operator gate or stage mutation")
    return updated
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--apply",action="store_true")
    args=p.parse_args()
    before=PATH.read_text(encoding="utf-8").replace("\r\n","\n")
    after=patch(before)
    if args.apply and after!=before:
        PATH.write_text(after,encoding="utf-8",newline="\n")
    elif not args.apply and after!=before:
        raise RuntimeError("V250 prepare reason logging not materialized")
    print("V250_RX33_RX34_EXACT_PMETAL_LERP_PREPARE_STAGE_"+("APPLIED" if args.apply else "VERIFIED"))
if __name__=="__main__":main()
