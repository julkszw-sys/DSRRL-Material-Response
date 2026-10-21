from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'src/operators/env_spec/pmetal_rgba_materializer.cpp').read_text()
l=(root/'src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp').read_text()
r=(root/'src/runtime/pmetal_envspec_draw_runtime.cpp').read_text()
assert "auto native_envdiffuse_source_audit = base;" in s
assert "outcome.envdiffuse_linear_consumer_diag = false;" in s
assert "outcome.envdiffuse_preserved = true;" in l
assert "preserved_envdiffuse" in l
assert "DSRRL_V230_STOCK_DSR_ENVDIFFUSE" in l
assert "native_s11_s13_untouched=1" in r
assert "prepared.native_dsr_envdiffuse_a" in r
assert "prepared.native_dsr_envdiffuse_b" in r
assert "prepared.env_resources.ptde_a" in r
assert "prepared.env_resources.ptde_b" in r
assert "prepared.request.additional_owners &= ~envdiff_owner;" in r
assert "prepared.request.additional_resource_owners = 0u;" in r
assert "outcome.spec_rgb_consumer = true;" in s and "materialize_spec_rgb_consumer" in l
print("PASS: stock DSR EnvDiffuse only, PTDE EnvSpec+MR+SpecRGB retained, exact producer fail-open")
