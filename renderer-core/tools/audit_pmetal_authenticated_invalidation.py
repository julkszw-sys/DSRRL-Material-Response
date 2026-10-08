#!/usr/bin/env python3
"""Static guard for the diagnostic authenticated producer invalidation cut.

No claim of PTDE pixel parity. Only the verified native owner transition may
revoke a previously published exact material source.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv)>1 else Path("renderer-core")
text = (root/"src/runtime/pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")
producer = text.split("void pmetal_env_source_selector_event(",1)[1].split("bool pmetal_env_source_runtime::latest(",1)[0]
tokens=[
    "pmetal_producer_state_clear();",
    "exact_pmetal_material_selection(",
    "const auto *descriptor =",
    "if (descriptor == nullptr ||",
    "const bool parent_verified =",
    "descriptor_owner != owner ||",
    "pmetal_producer_state_begin(",
    "const auto endpoints =",
    "if (!lookup_valid ||",
    "latest_hook_source_exact_selector(",
    "exact_envdiffuse_endpoint(",
    "pmetal_producer_state_publish(",
]
positions=[producer.find(item) for item in tokens]
if -1 in positions or positions!=sorted(positions):
    raise SystemExit("PMETAL_AUTHENTICATED_INVALIDATION_AUDIT_FAIL: semantic cut or donor gates changed")
if producer.count("pmetal_producer_state_begin(")!=1:
    raise SystemExit("PMETAL_AUTHENTICATED_INVALIDATION_AUDIT_FAIL: begin count")
if "next.diagnostic_origin = parent_verified ? 1u : 4u;" not in producer:
    raise SystemExit("PMETAL_AUTHENTICATED_INVALIDATION_AUDIT_FAIL: origin provenance")
consumer = text.split("bool pmetal_env_source_runtime::latest(",1)[1].split("pmetal_env_source_runtime::telemetry()",1)[0]
if "pmetal_producer_state_latest(" not in consumer or "latest_hook_source(out)" in consumer:
    raise SystemExit("PMETAL_AUTHENTICATED_INVALIDATION_AUDIT_FAIL: unkeyed fallback")
print("PMETAL_AUTHENTICATED_INVALIDATION_AUDIT_PASS")
