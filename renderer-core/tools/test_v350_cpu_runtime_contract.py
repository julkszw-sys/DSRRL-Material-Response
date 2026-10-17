#!/usr/bin/env python3
"""V350 release CPU-hot observer cuts: source-only, no renderer authorization edits."""
from pathlib import Path
r = Path(__file__).resolve().parents[2]
v287=(r/"renderer-core/include/dsrrl/runtime/v287_nonmetal_trace.hpp").read_text(encoding="utf-8")
v315=(r/"renderer-core/include/dsrrl/runtime/v315_direct_749_sidecar_ledger.hpp").read_text(encoding="utf-8")
builder=(r/"renderer-core/tools/build_v350_cpu_runtime_one_addon.ps1").read_text(encoding="utf-8")
integ=(r/"renderer-core/integrated/integrated_addon.cpp").read_text(encoding="utf-8")
env=(r/"renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
flag = "DSRRL_V350_RELEASE_SKIP_HOT_OBSERVERS"
def check(ok, message):
    if not ok:
        raise SystemExit("V350 CPU-RUNTIME FAIL: "+message)
def method(text, start, end):
    check(start in text and end in text, "method extraction "+start)
    return text.split(start,1)[1].split(end,1)[0]
check("/D"+flag in builder, "build must explicitly opt into no-observer production mode")
for feature in ("DSRRL_V350_PTDE_LERP_RGB_COMPONENT_SWIZZLE",
                "DSRRL_V293_PARSER_EXACT_FLVER_ALIAS=ON",
                "DSRRL_V285_ATOMIC_NONMETAL_DIAG=ON",
                "DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=ON",
                "DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE=ON",
                "DSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF",
                "DSRRL_FLVER_SELECTOR_PROFILE=OFF",
                "DSRRL_POINTLIGHT_PROFILE=OFF"):
    check(feature in builder, "renderer operator/profiling build drift "+feature)
for old in ("DSRRL_V290_FULL_SIDECAR_DRAW_CENSUS=ON", "DSRRL_V291_MATERIAL_STAGE_CENSUS=ON",
            "DSRRL_V291_LERP_CUBEMAP_CENSUS=ON", "Sleep(", "SetThreadAffinityMask("):
    check(old not in builder, "unexpected diagnostic/scheduling override "+old)
log=method(v287,"inline void log(", "} // namespace dsrrl::runtime::v287_nonmetal_trace")
check("#if defined("+flag+")" in log and "(void)m" in log and "return;" in log,
      "V287 hot material SHA folding and atomic hash dedup must be bypassed")
check("#else" in log and "#endif" in log and "v287_nonmetal_trace" not in log,
      "V287 opt-in diagnostic source remains present")
observe=method(v315,"inline void observe(", "inline void observe_probe(")
check("#if defined("+flag+")" in observe and "#else" in observe and "#endif" in observe
      and "return;" in observe and "count(original_logical)" in observe,
      "V315 exact 749 item observer, count and log are compiled out")
probe=method(v315,"inline void observe_probe(", "inline void prepared(")
check("#if defined("+flag+")" in probe and "#else" in probe and "#endif" in probe
      and "unidentifiable_draws.fetch_add" in probe,
      "V315 unresolved probe atomic counts need compile-time bypass")
check("observer=COMPILED_OUT runtime_draw_evidence=NOT_COLLECTED" in v315,
      "observer-off summary must not claim runtime coverage")
ledger_materializer=(r/"renderer-core/tools/materialize_v315_direct_749_runtime_ledger.py").read_text(encoding="utf-8")
check("v315::prepared(" in ledger_materializer and "v315::issued(" in ledger_materializer and
      "v287_nonmetal_trace::log(" in integ and "v287_nonmetal_trace::log(" in env,
      "leave downstream draw paths in place; no-op inline instrumentation only")
for feature in ("same_hook_source_payload(", "latest_hook_source(",
                "hook_producer_cache_lookup("):
    src=(r/"renderer-core/src/runtime/pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")
    check(feature in src, "protected PMetal source/selector "+feature)
print("V350 CPU-RUNTIME SOURCE PASS: exact PTDE operators intact, diagnostic-only V287/V315 hot observers compiled out, telemetry/QPC OFF")
