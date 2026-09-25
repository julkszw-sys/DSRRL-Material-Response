#!/usr/bin/env python3
"""Guard PTDE FLVER census completeness claims.

Construction evidence only. This verifier deliberately does not promote runtime
activation, pixel equivalence, resource-byte identity, or cross-version homology.
A corpus with parser/scan failures may still provide positive USE evidence, but
absence in that corpus must remain UNKNOWN/fail-open.
"""
from __future__ import annotations
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FULL = ROOT / "data" / "ptde_full_flver_ownership_census_v1.json"
SEM = ROOT / "data" / "census" / "ptde_flver_texture_semantics_v1_source.json"

full = json.loads(FULL.read_text(encoding="utf-8"))
sem = json.loads(SEM.read_text(encoding="utf-8"))

errors = int(full["counts"].get("scan_errors", 0))
sem_errors = int(sem["source"].get("scan_error_count", 0))
assert errors == sem_errors, (errors, sem_errors)
assert sem.get("positive_use_only") is True
assert sem.get("absence_is_unknown") is True
assert sem.get("runtime_policy", {}).get("missing_capability") == "UNKNOWN_FAIL_OPEN"
assert sem.get("runtime_policy", {}).get("nonjoined_identity") == "UNKNOWN_FAIL_OPEN"

# A clean source-complete semantic census requires zero parser/scan failures.
# Historical manifests may retain a SOURCE_COMPLETE_OWNER_CORPUS label meaning
# the supplied archive set itself was complete; that label must not be consumed
# as semantic negative-evidence authority while errors remain.
semantic_negative_authority = errors == 0
if errors:
    assert sem.get("coverage_state") == "PARTIAL_SOURCE_COVERAGE"
    assert not semantic_negative_authority

# Keep construction separate from runtime/pixel claims.
assert full.get("runtime_status") == "OPEN"
assert full.get("pixel_status") == "OPEN"
assert full.get("resource_byte_identity") == "OPEN"
assert full.get("cross_version_homology") == "OPEN"

print(json.dumps({
    "ptde_flver_scan_errors": errors,
    "semantic_negative_authority": semantic_negative_authority,
    "absence_policy": "UNKNOWN_FAIL_OPEN" if errors else "eligible_for_separate_evidence_review",
    "construction_only": True,
}, sort_keys=True))
