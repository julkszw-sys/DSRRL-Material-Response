#!/usr/bin/env python3
"""Freeze the owner-tested first v2.3.0 cleanup against non-P_Metal regressions.

User runtime evidence: non-P_Metal routes worked in CI39096862827 binary, and
broke in later v2.3.0 telemetry-clean builds. This is a provenance guard,
NOT a pixel test or proof of cause.
"""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
b=(root/"tools/build_v230_production.ps1").read_text("utf8")
v=json.loads((root/"integrated/VERSION.json").read_text("utf8"))
assert v["version"]=="2.3.0"
for feature in ("DSRRL_V298_LIVE_MTD_WITNESS",
                "DSRRL_V314_UNROUTED_LIT_MUL_CENSUS",
                "DSRRL_V315_DIRECT_749_RUNTIME_LEDGER"):
    parent=(root/"tools/build_v350_gpu_resident_pointlight_off_rc2.ps1").read_text("utf8")
    assert "/D"+feature in parent, "Owner-tested baseline compile feature missing"
    assert "Once ' /D"+feature+"' ''" not in b, "Regression-era census opt-out present"
for stage in range(2,7):
    assert "materialize_v230_release_cleanup_stage"+str(stage)+".py" not in b
for anchor in ("materialize_v230_release_cleanup.py",
               "test_v230_release_contract.py",
               "audit_active_islands_hotpath.py"):
    assert anchor in b
assert "DSRRL_QPC_SCOPE_SHELL_NO_CLOCK" in (root/"tools/build_v350_gpu_resident_pointlight_off_rc2.ps1").read_text("utf8")
s=(root/"src/runtime/pmetal_env_source_runtime.cpp").read_text("utf8")
assert "latest_hook_source" in s
assert "g_hook_producer_readonly_hit_enabled" not in s
core=(root/"integrated/integrated_addon.cpp").read_text("utf8")
assert "g_material_response." in core
assert "material_receiver_runtime_slot" in core
assert "evaluate_direct_pointlight_material" in core
print("V230_FIRST_CLEAN_OWNER_GOOD_SOURCE_GUARD_PASS")
print("Nonmetal V298/V314/V315 preserved from owner-tested binary; V350/V359 source and caches protected")
