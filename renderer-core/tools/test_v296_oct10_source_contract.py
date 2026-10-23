#!/usr/bin/env python3
"""Construction-only V296 proof; runtime/pixel remain OPEN."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
src = (root/"renderer-core/integrated/integrated_addon.cpp").read_text()
patcher = (root/"renderer-core/tools/materialize_v296_oct10_atomic_policy.py").read_text()
gate = (root/"renderer-core/include/dsrrl/runtime/v296_oct10_material_atomic_gate.hpp").read_text()
manifest = (root/"renderer-core/data/provenance/sidecar_specular_20261010_names.b85")
assert manifest.exists()
assert src.count("    const bool diffuse_mr_active =\n        decision.active &&") == 1
assert src.count("    if (context != nullptr) {\n        // MR reset invariant: generic stable MR is diffuse-v1 only.") == 1
assert "probe_exact_specular_companion(context)" in patcher
assert "!v296_partial_material" in patcher
assert "stock_t1_bound" in gate and "oct10_manifest_member" in gate
assert "companion_ready" not in gate.split("constexpr material_atomic_policy decide_material_policy")[1].split("}")[0]
assert "exact_pmetal_envspec_candidate(" in src
assert "upper_lower_bound" in src
print("V296_SOURCE_CONSTRUCTION_PASS")
print("V296_NATIVE_BUILD_RUNTIME_PIXEL_OPEN")
