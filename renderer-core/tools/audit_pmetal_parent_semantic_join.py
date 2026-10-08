#!/usr/bin/env python3
"""Construction guard for the parent-fingerprint diagnostic.

This is NOT pixel validation. The alternate source must still pass the same
native selector/owner/LightBank and exact PTDE EnvDiffuse sidecar gates.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("renderer-core")
s = (root / "src/runtime/pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")
h = (root / "include/dsrrl/runtime/pmetal_env_source_runtime.hpp").read_text(encoding="utf-8")
cmake = (root / "integrated/CMakeLists.txt").read_text(encoding="utf-8")

def require(sub, text, what):
    if sub not in text:
        raise SystemExit("PARENT_SEMANTIC_JOIN_AUDIT_FAIL: missing " + what)

def order(text, items):
    offsets = [text.find(item) for item in items]
    if any(x < 0 for x in offsets) or offsets != sorted(offsets):
        raise SystemExit("PARENT_SEMANTIC_JOIN_AUDIT_FAIL: downstream gate order changed")

require("DSRRL_PMETAL_PARENT_SEMANTIC_JOIN_DIAG", cmake, "feature gate")
require("DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG", cmake, "sidecar prerequisite")
require("std::uint8_t diagnostic_origin", h, "per-source provenance carrier")
producer = s.split("void pmetal_env_source_selector_event(", 1)[1].split("bool pmetal_env_source_runtime::latest(", 1)[0]
order(producer, [
    "exact_pmetal_material_selection(",
    "const bool parent_verified =",
    "#if !defined(DSRRL_PMETAL_PARENT_SEMANTIC_JOIN_DIAG)",
    "descriptor_owner != owner",
    "if (!endpoints.valid)",
    "if (!lookup_valid",
    "latest_hook_source_exact_selector(",
    "pmetal_producer_state_publish(",
])
require("exact_envdiffuse_endpoint(", producer, "PTDE sidecar")
require("next.diagnostic_origin = parent_verified ? 1u : 4u", producer, "alternate provenance")
consumer = s.split("bool pmetal_env_source_runtime::latest(", 1)[1].split("pmetal_env_source_runtime::telemetry()", 1)[0]
require("pmetal_producer_state_latest(", consumer, "exact material authority")
if "latest_hook_source(out)" in consumer:
    raise SystemExit("PARENT_SEMANTIC_JOIN_AUDIT_FAIL: unqualified source fallback")
require("out.diagnostic_origin != 4u", consumer, "preserved recovered origin")
print("DSRRL_PMETAL_PARENT_SEMANTIC_JOIN_AUDIT_PASS")
