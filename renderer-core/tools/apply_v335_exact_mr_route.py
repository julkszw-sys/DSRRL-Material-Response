#!/usr/bin/env python3
"""V335: deterministic, narrow source materialization for authenticated MR routes.

Never patch the retail executable or alter independent SpecRGB/EnvSpec/PointLight.
Apply to integrated_addon.cpp after all older materializers, prior to Win64 build.
"""
from __future__ import annotations
import argparse
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/"integrated/integrated_addon.cpp"
INCLUDE='#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"'
NEW_INCLUDE=INCLUDE+'''
#if defined(DSRRL_V335_MR_EXACT_OWNER_ROUTE)
#include "dsrrl/runtime/v335_mr_exact_owner_route.hpp"
#endif'''
DECISION='''    out_decision =
        direct_pointlight_receiver
            ? g_material_response.'''
PREFIX='''#if defined(DSRRL_V335_MR_EXACT_OWNER_ROUTE)
    // The draw already carries the real MTD hash, but legacy owner routing
    // can have route=0. Repair only when the original SHA and complete
    // FLVER+slot+semantic identity match a certified standard MR profile.
    // Never touch direct PointLight, and never authorize a resource bridge.
    const bool v335_route_recovered =
        !direct_pointlight_receiver &&
        dsrrl::runtime::v335_recover_exact_mr_route(
            out_material, receiver_id);
#endif

'''
POST='''#if defined(DSRRL_V335_MR_EXACT_OWNER_ROUTE)
    if (v335_route_recovered) {
        // Log just once per relevant route family, never per-indexed draw.
        static std::atomic_bool once_dull{false};
        static std::atomic_bool once_edge{false};
        static std::atomic_bool once_body{false};
        static std::atomic_bool once_other{false};
        std::atomic_bool *once = &once_other;
        if (out_material.route_index == 160u) once = &once_dull;
        else if (out_material.route_index == 88u) once = &once_edge;
        else if (out_material.route_index == 232u) once = &once_body;
        if (!once->exchange(true, std::memory_order_relaxed)) {
            char line[320]{};
            std::snprintf(line, sizeof(line),
                "[DSRRL V335 MR ROUTE] stage=exact_route_recovered "
                "rx=%u route=%u active=%u reason=%u slot=%u "
                "mtd=%02x%02x%02x%02x flver=%02x%02x%02x%02x",
                receiver_id, out_material.route_index,
                out_decision.active ? 1u : 0u,
                static_cast<unsigned>(out_decision.reason),
                out_material.material_slot,
                out_material.raw_mtd_sha256[0],
                out_material.raw_mtd_sha256[1],
                out_material.raw_mtd_sha256[2],
                out_material.raw_mtd_sha256[3],
                out_material.flver_sha256[0],
                out_material.flver_sha256[1],
                out_material.flver_sha256[2],
                out_material.flver_sha256[3]);
            reshade::log::message(reshade::log::level::info, line);
        }
    }
#endif

'''
AFTER='''    if (out_decision.active) {
        hot_count(g_mr_would_activate);'''

def patch(s: str)->str:
    if NEW_INCLUDE in s and PREFIX in s and POST in s:
        return s
    if s.count(INCLUDE)!=1 or s.count(DECISION)!=1:
        raise RuntimeError("V335 original source anchor changed (fail closed)")
    s=s.replace(INCLUDE,NEW_INCLUDE,1)
    s=s.replace(DECISION,PREFIX+DECISION,1)
    pos=s.find(AFTER,s.find(PREFIX)+len(PREFIX))
    if pos<0 or s.count(AFTER)!=2:
        raise RuntimeError("V335 MR post-decision marker changed (fail closed)")
    return s[:pos]+POST+s[pos:]

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--apply",action="store_true")
    ap.add_argument("--check",action="store_true")
    opt=ap.parse_args()
    old=SOURCE.read_text(encoding="utf-8")
    new=patch(old)
    if opt.apply and old!=new:
        SOURCE.write_text(new,encoding="utf-8")
    if opt.check:
        if old!=new: raise RuntimeError("V335 source was not materialized before check")
        assert new.count("v335_recover_exact_mr_route(")==1
        assert new.count("v335_route_recovered")==2
    print("V335 MR exact owner route source", "MATERIALIZED" if opt.apply else "CHECKED",
          "idempotent",patch(new)==new)
if __name__=="__main__":
    main()
