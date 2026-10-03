#!/usr/bin/env python3
from pathlib import Path
import sys

root=Path(sys.argv[1] if len(sys.argv)>1 else "renderer-core")
cmake=(root/"integrated"/"CMakeLists.txt").read_text(encoding="utf-8")
source=(root/"src"/"runtime"/"pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")
hooks=(root/"src"/"runtime"/"flver_engine_hooks.cpp").read_text(encoding="utf-8")
draw=(root/"src"/"runtime"/"pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
resources=(root/"src"/"runtime"/"material_resource_draw_runtime.cpp").read_text(encoding="utf-8")
stable=(root/"src"/"operators"/"env_spec"/"pmetal_rgba_materializer.cpp").read_text(encoding="utf-8")
lerp=(root/"src"/"operators"/"env_spec"/"pmetal_rgba_lerp_materializer.cpp").read_text(encoding="utf-8")

checks={
"cmake":[
"DSRRL_EQUIPMENT_WIDE_PTDE_SURFACE_RUNTIME",
"DSRRL_PMETAL_V13_NATIVE_DSR_MATERIAL_MOD_DIAG",
],
"source":[
"exact_envspec_source_material_selection",
'mtd_semantic_hash("DifSpcBmp")',
"classify_mtd_envspec_semantics",
"ptde_envspec_presence::present",
"env.envspc_slot < 4u",
],
"hooks":[
"DSRRL_EQUIPMENT_WIDE_PTDE_SURFACE_RUNTIME",
'mtd_semantic_hash("DifSpcBmp")',
"pmetal_env_source_selector_event",
],
"draw":[
"DSRRL_EQUIPMENT_WIDE_PTDE_SURFACE_RUNTIME",
'material.material_family_hash !=\n            mr::mtd_semantic_hash("DifSpcBmp")',
"env.envspc_slot < 4u",
"[DSRRL EQUIPMENT PTDE SURFACE]",
"[DSRRL EQUIPMENT SURFACE ACTIVE]",
"prepared.material_resources.spec_rgb",
],
"resources":[
"DSRRL_EQUIPMENT_WIDE_PTDE_SURFACE_RUNTIME",
"mtd_semantic_operator::env_spec",
"exact_name_ptde_companion_verified",
"native_t10_transport_ready",
],
"stable":[
"apply_v13_native_dsr_no_tail",
"apply_v13_native_dsr_material_mod_only",
"compose_exact_terminal_rgb_sat",
"outcome.spec_rgb_consumer = true",
],
"lerp":[
"apply_v13_lerp_material_mod_only",
"apply_exact_terminal_rgb_sat",
"outcome.terminal_sat_rgb_composed = true",
"outcome.spec_rgb_consumer = true",
]
}
texts={"cmake":cmake,"source":source,"hooks":hooks,"draw":draw,"resources":resources,"stable":stable,"lerp":lerp}
for group,needles in checks.items():
    for needle in needles:
        if needle not in texts[group]:
            raise SystemExit(f"MISSING {group}: {needle}")

# Anti-hybrid assertions: the equipment profile must compile the existing
# V13 material-mod island, whose postconditions remove the DSR t9 split-sum.
for needle in ("t9_sample == 0u","t10_sample == 1u"):
    if needle not in stable:
        raise SystemExit(f"stable anti-hybrid postcondition missing: {needle}")

# The generic resource router may authorize SpecRGB in this profile only
# through an explicit EnvSpec consumer, never generic MR/F0 ownership.
equip_block=resources[resources.index("#if defined(DSRRL_EQUIPMENT_WIDE_PTDE_SURFACE_RUNTIME)"):]
equip_block=equip_block[:equip_block.index("#else")]
if "mtd_semantic_operator::env_spec" not in equip_block:
    raise SystemExit("equipment SpecRGB is not tied to explicit EnvSpec semantics")
if "mtd_semantic_operator::spec_rgb" in equip_block:
    raise SystemExit("equipment branch unexpectedly depends on generic spec_rgb semantic")

# Physical policy must remain untouched.
for forbidden in ("operator_id::subsurface:\n        return true",
                  "operator_id::upper_lower:\n        return true",
                  "operator_id::hemdir3:\n        return true"):
    if forbidden in draw:
        raise SystemExit(f"forbidden owner re-enable: {forbidden}")

print("EQUIPMENT_WIDE_PTDE_SURFACE_RUNTIME_AUDIT_PASS")
