#!/usr/bin/env python3
"""Inject ONLY certified EnvSpec-only PS registration into compiled physical cut.

CMake generates the integrated physical-cut unit at configure-time.
Subsequent V230 materializers add instrumentation and feature code directly to
that generated unit. Regenerating it destroys those overlays. Copy just the
new, exact stable HemEnv registration block from materialized source instead.
"""
from pathlib import Path
original=Path("renderer-core/integrated/integrated_addon.cpp")
compiled=Path("build-v230-release/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp")
src=original.read_text(encoding="utf-8")
dst=compiled.read_text(encoding="utf-8")
anchor="                    ++g_envspec_payload_materialize_ok;"
assert src.count(anchor)==2,("ORIGINAL_STABLE_ANCHOR_DRIFT",src.count(anchor))
assert dst.count(anchor)>=1,("COMPILED_STABLE_ANCHOR_DRIFT",dst.count(anchor))
pos=src.index(anchor)
start=src.rfind("#if defined(DSRRL_V230_PMETAL_ENV_ONLY_ISLAND)",0,pos)
assert start>=0,("NO_ENV_ONLY_CREATE_BLOCK",)
block=src[start:pos]
assert "[DSRRL ENV ONLY CREATE]" in block
assert "register_envspec_only_replacement" in block
assert "envspec.receiver_id" in block
assert "[DSRRL ENV ONLY CREATE]" not in dst
assert "[DSRRL EQUIPMENT ROUTE CENSUS]" in dst,("EQUIPMENT_CENSUS_PRESERVATION_REQUIRED",)
assert "[DSRRL HELM ENTRY CUT]" in dst,("HELM_PRESERVATION_REQUIRED",)
dst=dst.replace(anchor,block+anchor,1)
assert dst.count("[DSRRL ENV ONLY CREATE]")==1
assert "[DSRRL EQUIPMENT ROUTE CENSUS]" in dst
assert "[DSRRL HELM ENTRY CUT]" in dst
compiled.write_text(dst,encoding="utf-8")
print("V230_PHYSICAL_CUT_ENV_ONLY_CREATE_PASS exact insertion; original equipment and helmet observers retained")
