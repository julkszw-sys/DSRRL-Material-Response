#!/usr/bin/env python3
"""v2.3.1 release source/builder contract; no runtime/pixel claim.

The source commit is the exact user-uploaded B235 ancestor plus fully
reconstructible Warrior resource/MTD/cache/V296 patches. Only the
opt-in diagnostic marker V13 LOADER CUT is compiled out; resource
acquisition is still applied before native compilation.
"""
import json
from pathlib import Path
r=Path("renderer-core")
v=json.loads((r/"integrated/VERSION.json").read_text("utf8"))
assert v["version"]=="2.3.1"
assert v["artifact_stem"]=="DSRRL_Restored_Lighting_v2_3_1"
assert v["status"]=="RELEASE_PREPARATION"
b=(r/"tools/build_v230_production.ps1").read_text("utf8")
for item in ("DSRRL_Restored_Lighting_v2.3.1.addon64",
             "/DDSRRL_V230_PRODUCTION",
             "materialize_v230_release_cleanup.py",
             "materialize_v230_armor_v13_exact_supplement.py",
             "materialize_v230_v13_target_loader_closure.py",
             "materialize_b235_remove_v296_unproven_crossgate.py",
             "test_b235_warrior_recovery.py"):
    assert item in b,item
assert "/DDSRRL_V230_V13_TARGET_LOADER_CLOSURE" not in b
assert "2.3.0.addon64" not in b
loader=(r/"tools/materialize_v230_v13_target_loader_closure.py").read_text("utf8")
for name in ("armor_v13::diffuse_name(logical_hash)","armor_v13::normal_name(logical_hash)"):
    assert name in loader,name
assert "#if defined(DSRRL_V230_V13_TARGET_LOADER_CLOSURE)" in loader
assert "[DSRRL V13 LOADER CUT]" in loader
# Above reporter exists solely in a #if OFF block; checker prohibits
# the enabling compile flag while retaining exact loader corrections.
h=(r/"include/dsrrl/runtime/runtime_hot_telemetry.hpp").read_text("utf8")
assert "inline bool hot_enabled()" in h
assert "inline bool effect_enabled()" in h
assert "inline bool native_state_verification_enabled()" in h
source=(r/"src/runtime/pmetal_env_source_runtime.cpp").read_text("utf8")
assert "latest_hook_source" in source
c=(r/"src/runtime/material_response_draw_transaction.cpp").read_text("utf8")
assert "replacement_key_published_[key_index]" in c
assert "bank_index * k_replacement_key_receivers" in c
assert not any(x in b for x in (
    "/DDSRRL_V352_LONG_SOAK_PRESENT",
    "/DDSRRL_V353_CPU_MODULE_PROFILER",
    "/DDSRRL_V296_ROOTCAUSE_TELEMETRY"))
print("V231 release source PASS: B235 source lineage, version 2.3.1, V13 loader retained, diagnostics OFF")
print("CI construction is not runtime/actor pixel proof; historical external DDS sidecars required")
