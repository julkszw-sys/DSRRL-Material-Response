from pathlib import Path
root=Path("renderer-core")
hdr=(root/"include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp").read_text()
src=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
addon=(root/"integrated/integrated_addon.cpp").read_text()
assert "bool stable_envdiffuse_trace_valid = false;" in hdr
assert src.count("prepared.stable_envdiffuse_source_serial = source.serial;")==2
assert src.count("prepared.stable_envdiffuse_c86 = source.envdiffuse_a;")==2
assert src.count("prepared.stable_envdiffuse_probe =")==2
assert addon.count("[DSRRL V388 STABLE GPU JOIN]")==1
assert "stage=indexed_issued" in addon
assert "source_to_draw=1 pixel=OPEN" in addon
assert "if(issued && !hemenvlerp_bound)" in addon
assert "v388_envdiffuse_stable_issued(" in addon
assert addon.index("const bool issued =") < addon.index("v388_envdiffuse_stable_issued(\n                material,decision,prepared.envspec)")
assert "flver_sha=%s mtd_sha=%s" in addon
assert "static thread_local std::array<sample,256u> recent{}" in addon
assert "now-slot.tick<2000ull" in addon
assert "apply_mutation(" not in addon[addon.index("void v388_envdiffuse_stable_issued("):addon.index("bool prepare_island_batch(")]
print("V388 issuer exact source-to-STABLE-draw trace contract PASS")
