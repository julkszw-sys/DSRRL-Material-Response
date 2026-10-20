#!/usr/bin/env python3
"""Static cross-operator mapping audit for v2.3.0 / PR482, fail closed on drift.

This audits source routing contracts, *not* in-game GPU or pixel equivalence.
Most importantly, do NOT resurrect rejected V319/V320 21-slot PTDE guesses:
the corrected original BND3 parser (header +0x20 vs erroneous +0x2c)
established the shared PTDE and DSR EnvSpcSlotNo values match.
"""
from __future__ import annotations
import json, re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
def load(path: str) -> str:
    return (ROOT/path).read_text(encoding="utf-8")

legacy=load("renderer-core/include/dsrrl/operators/env_spec/legacy_resource_bridge.hpp")
cube=load("renderer-core/src/runtime/envspec_resource_runtime.cpp")
mtd=load("renderer-core/src/operators/material_response/mtd_semantic_census.cpp")
lerp=load("renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
draw=load("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
b230=load("renderer-core/tools/build_v230_production.ps1")
base=load("renderer-core/tools/build_v350_gpu_resident_pointlight_off_rc2.ps1")
router=json.loads(load("renderer-core/data/census/ptde_mtd_envspec_router_v1.json"))["records"]
diagnostic=load("renderer-core/tools/materialize_v230_unknown_dsr_probe_experiment.py")
results=[]

def check(label:str, condition:bool, meaning:str, limitation:str=""):
    results.append({"scope":label,"source_contract":"PASS" if condition else "FAIL",
                    "meaning":meaning,"limitation":limitation})
    if not condition:
        raise AssertionError(label+": source/regression contract FAILED")

def compact(s:str)->str:
    return re.sub(r"\s+","",s)

check("REJECTED V319/V320 phantom MTD slot corrections",
      "DSRRL_V320_PTDE_ALL_EXACT_ENVSPEC_SLOTS" not in b230
      and "DSRRL_V319_PTDE_SLOT" not in b230
      and "DSRRL_V230_PTDE_ENVSPEC_SLOT21" not in b230
      and "v230_exact_ptde_slot" not in mtd
      and "return envspec_from_router_record(s);" in mtd,
      "Production retains exact original DSR MTD slot; historical 21 PTDE slot overrides were REJECTED by corrected BND3 extraction.",
      "PTDE BND3 original pairwise evidence carried by V322 finding, not re-extracted by this script.")

check("Original MTD source identities",
      len(router)==325 and
      sum(x["state"]=="PRESENT" for x in router)==213 and
      sum(x["state"]=="EXPLICIT_NONE" for x in router)==87 and
      sum(x["state"]=="NOSPC_HOST" for x in router)==25 and
      len({(x["mtd_name"],x["dsr_mtd_sha256"]) for x in router})==325 and
      "q.material.raw_mtd_sha256==s.raw_mtd_sha256" in mtd,
      "All 325 MTD names have unique SHA-pinned DSR identities and exactly categorized original states.",
      "Source census identity does not prove per-draw material ownership or PTDE pixels.")

check("Native probe identity and PTDE atlas layout",
      "k_legacy_envspec_probe_count = 342u" in legacy
      and "k_legacy_envspec_endpoint_count = 4u" in legacy
      and "k_legacy_packed_gi_size = 33619968ull" in legacy
      and "k_ptde_slots = 4u" in cube
      and "k_ptde_cube_bytes" in cube
      and compact(cube).count("probe_a)*k_ptde_slots+slot;")>=1
      and "legacy_native_probe_for_sha" in cube,
      "Exact native BC6H SHA -> certified probe ordinal, index=probe*4+slot, 1368 RGBA cube records.",
      "Source-level only; GPU readback and original binder bytes not independently repeated.")

check("Native cubemap fallback boundaries",
      "DSRRL_V230_UNKNOWN_DSR_PROBE_DIAG" in b230
      and "k_v230_unknown_stock_shas" in diagnostic
      and "g_v230_unknown_native_views" in diagnostic
      and "experimental_unknown_native" in diagnostic
      and "PREPARED_NOT_PIXEL" in diagnostic,
      "Optional experimental DSR native t12/t14 resources are separate from certified PTDE ordinal lookup.",
      "Three evidence-backed hashes only. Exact draw issue and original resource producer remain open.")

check("SpecRGB t1 resource identity and fail-open",
      "[DSRRL SPECRGB ROOT]" in draw
      and "logical_known=" in draw
      and "companion_ready=" in draw
      and "spec_rgb_reject" in draw
      and "prepared.env_resources);" in draw,
      "Unknown t1 source/companion rejects the PTDE material/resource transaction after EnvSpec preparation.",
      "Confirmed in owner log; legitimate stock fail-open, NOT proof of incorrect MTD/cubemap mapping.")

check("EnvDiffuse intentionally stock DSR",
      "DSRRL_V230_STOCK_DSR_ENVDIFFUSE" in b230
      and "DSRRL_V230_STOCK_DSR_ENVDIFFUSE" in lerp
      and "DSRRL_V230_STOCK_DSR_ENVDIFFUSE" in draw,
      "Stock DSR t11/t13 path retained by v2.3.0 isolation; PTDE EnvDiffuse rewrite gated off.",
      "Not PTDE EnvDiffuse operator parity; must not misreport stock behavior as source corruption.")

check("HemEnvLerp source beta ABI",
      "source.beta" in draw and "source.a[0]" in draw and
      "source.b[0]" in draw and "source.envdiffuse_b[0]" in draw
      and "0x0b000032u" in lerp and
      "k_ptde_rgba_envspec_chain" in lerp
      and "probe_b_required" in draw,
      "CB12 writer receives A,B,beta and exact shader contains A/B sample and MAD interpolation.",
      "Does NOT prove GPU CB12 value is consumed, Stable endpoint continuity, or final HDR pixels.")

check("No unreviewed PointLight operator reintroduction",
      "-DDSRRL_PHYSICAL_CUT_POINTLIGHT_ALL=ON" in base
      and "-DDSRRL_POINTLIGHT_DRAWTIME_BYPASS=ON" in base
      and "-DDSRRL_POINTLIGHT_PROFILE=OFF" in base,
      "Addon PointLight operator physically disabled in baseline; stock host operator remains an independent concern.",
      "Not a test of PTDE versus DSR PointLight spatial attenuation or pixels.")

check("Known SpecRGB swizzle audit remains wired",
      "audit_v338_nonmetal_lerp_else_rgb.py" in base
      and "DSRRL_V349_PTDE_LERP_ENVSPEC_YZW_FIX" in base,
      "Nonmetal HemEnvLerp per-channel register mapping carries historical V338/V349 regression checks.",
      "Compilation contract only; per-material visible behavior still OPEN.")

if __name__=="__main__":
    out={"schema":"dsrrl_cross_operator_audit_v1","scope":"v230 source, PR482 base SHA 457d8d93",
         "contract_pass_count":len(results),"contract_fail_count":0,
         "authoritative_correction":"V320 21 MTD changes REJECTED by later V322 original BND3 parser correction",
         "findings":results}
    p=ROOT/"operator_mapping_audit_v230.json"
    p.write_text(json.dumps(out,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    for item in results:
        print(item["source_contract"],item["scope"])
    print("OPEN: full original PTDE operator/pixel equivalence, runtime SpecRGB source and HemEnvLerp beta continuity")
