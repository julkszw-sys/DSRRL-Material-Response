from pathlib import Path
root=Path("renderer-core")
addon=(root/"integrated/integrated_addon.cpp").read_text()
v387=(root/"include/dsrrl/runtime/v387_stable_envdiffuse_cut.hpp").read_text()
source=(root/"src/runtime/pmetal_env_source_runtime.cpp").read_text()
build=(root/"tools/build_v230_production.ps1").read_text()
fn=addon[addon.index("void v388_envdiffuse_stable_issued("):addon.index("bool prepare_island_batch(")]
assert "source1_zero_c86_total" in fn and "source2_zero_c86_total" in fn
assert "[DSRRL V397 SOURCE COUNTERS]" in fn
assert "nth%8192u" in fn
assert "if(!initial && !pulse && !first_source2 && !first_zero_source1)" in fn
assert fn.index("if(!initial && !pulse && !first_source2 && !first_zero_source1)")<fn.index("char flver[65]")
assert "source1_zero_c86" in fn and "selector_owner_join=OPEN" in fn
assert "nth>24u && (nth & (nth-1u))!=0u" in v387
assert v387.index("nth>24u")<v387.index("char flver_sha[65]")
assert "/DDSRRL_V396_STRICT_NATIVE_SOURCE1" in build
assert "latest_hook_source_exact_selector(" in source
assert "latest_hook_source(out)" in source
assert "v388_envdiffuse_stable_issued(" in addon
print("V397 low-noise Source1 counters, rate limiting and V396 compatibility PASS")
