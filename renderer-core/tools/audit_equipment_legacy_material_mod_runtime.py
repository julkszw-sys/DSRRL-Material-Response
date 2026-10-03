#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else "renderer-core")
mat = (root/"src"/"operators"/"resource_bridges"/"equipment_legacy_spec_materializer.cpp").read_text(encoding="utf-8")
hdr = (root/"include"/"dsrrl"/"runtime"/"material_response_draw_transaction.hpp").read_text(encoding="utf-8")
tx = (root/"src"/"runtime"/"material_response_draw_transaction.cpp").read_text(encoding="utf-8")
addon = (root/"integrated"/"integrated_addon.cpp").read_text(encoding="utf-8")
cmake = (root/"integrated"/"CMakeLists.txt").read_text(encoding="utf-8")

required = {
    "materializer": [
        "materialize_spec_rgb_consumer",
        "find_color0_register",
        "k_float_2_2",
        "find_angular_chain",
        "find_t9_split_sum_instruction",
        "no",
        "0x00101246u",
        "12u,\n            2u",
        "c101_material_mul == 1u",
        "t9_samples == 0u",
        "t10_dst",
    ],
    "header": [
        "register_equipment_spec_replacement",
        "register_equipment_lerp_spec_replacement",
        "promote_prevalidated_equipment_to_spec_rgb",
        "equipment_spec_replacements_",
        "equipment_lerp_spec_replacements_",
    ],
    "transaction": [
        "replacement_bank::equipment_spec",
        "replacement_bank::equipment_lerp_spec",
        "promote_prevalidated_equipment_to_spec_rgb",
    ],
    "addon": [
        "DSRRL_EQUIPMENT_LEGACY_MATERIAL_MOD_RUNTIME",
        "materialize_equipment_legacy_spec_response",
        "[DSRRL EQUIPMENT LEGACY SPEC]",
        "family=HemEnv",
        "family=HemEnvLerp",
    ],
    "cmake": [
        "DSRRL_EQUIPMENT_LEGACY_MATERIAL_MOD_RUNTIME",
        "equipment_legacy_spec_materializer.cpp",
    ],
}
texts={"materializer":mat,"header":hdr,"transaction":tx,"addon":addon,"cmake":cmake}
for group, needles in required.items():
    for needle in needles:
        if needle not in texts[group]:
            raise SystemExit(f"MISSING {group}: {needle}")

# The equipment path must not consume the P_Metal source runtime.
if "pmetal_env_source_runtime" in mat or "latest_hook_source" in mat:
    raise SystemExit("equipment materializer illegally depends on P_Metal source")

if "materialize_spec_rgb_consumer(\n            source,size,spec_base,true)" in mat:
    raise SystemExit("equipment materializer resurrected legacy b12[0] c101 helper ABI")

# Exact SpecRGB draw resource is authorized only in the compile-scoped path.
if "#if defined(DSRRL_EQUIPMENT_LEGACY_MATERIAL_MOD_RUNTIME)" not in addon:
    raise SystemExit("equipment runtime is not compile-scoped")

print("EQUIPMENT_LEGACY_MATERIAL_MOD_AUDIT_PASS")
