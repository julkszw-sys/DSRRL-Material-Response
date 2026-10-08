#!/usr/bin/env python3
"""Guard the P_Metal diagnostic-only reject taxonomy against changing material authority."""
from pathlib import Path
import sys
root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("renderer-core")
s = (root / "src/runtime/pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")
selector = s.split("void pmetal_env_source_selector_event(",1)[1].split("bool pmetal_env_source_runtime::latest(",1)[0]
required = [
  "g_selector_authenticated_begin",
  "g_selector_shadow_publish",
  "g_selector_fallback_publish",
  "selector_source_a_read",
  "selector_source_b_read",
  "selector_envdiffuse_a",
  "selector_envdiffuse_b",
  "selector_manager_invalid",
  "selector_source_pointer_invalid",
  "selector_table_invalid",
  "g_selector_last_read_decode_stage",
  "record_selector_decode_miss(",
  "pmetal_producer_state_begin(",
  "pmetal_producer_state_publish(",
  "exact_envdiffuse_endpoint(",
]
for token in required:
    if token not in s:
        raise SystemExit("PMETAL_SELECTOR_DECODE_FRONTIER_AUDIT_FAIL: missing "+token)
if selector.count("pmetal_producer_state_begin(") != 1 or selector.count("pmetal_producer_state_publish(") != 2:
    raise SystemExit("PMETAL_SELECTOR_DECODE_FRONTIER_AUDIT_FAIL: changed producer contract")
def order(*tokens):
    places = [selector.find(t) for t in tokens]
    return all(p >= 0 for p in places) and places == sorted(places)
if not order("descriptor_owner != owner", "pmetal_producer_state_begin(", "if (!endpoints.valid)", "if (!lookup_valid ||", "latest_hook_source_exact_selector(", "if (!decode(", "exact_envdiffuse_endpoint(", "pmetal_producer_state_publish(\n        material,"):
    raise SystemExit("PMETAL_SELECTOR_DECODE_FRONTIER_AUDIT_FAIL: source authority order changed")
if "latest_hook_source(out)" in s.split("bool pmetal_env_source_runtime::latest(",1)[1].split("pmetal_env_source_runtime::telemetry()",1)[0]:
    raise SystemExit("PMETAL_SELECTOR_DECODE_FRONTIER_AUDIT_FAIL: unkeyed fallback restored")
if "#if defined(DSRRL_PMETAL_SELECTOR_DECODE_FRONTIER_DIAG)" not in s:
    raise SystemExit("PMETAL_SELECTOR_DECODE_FRONTIER_AUDIT_FAIL: no diagnostic compile gate")
print("PMETAL_SELECTOR_DECODE_FRONTIER_AUDIT_PASS")
