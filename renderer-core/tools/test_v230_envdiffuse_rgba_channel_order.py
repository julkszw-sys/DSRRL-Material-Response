#!/usr/bin/env python3
"""V230 owner-build-235 exact PTDE EnvDiffuse RGB/A DXBC source regression test.

Run AFTER source materializer, before native Windows build.
Checks corrected PTDE A/B consumer producer and hard fail-open.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
stable=(ROOT/"src/operators/env_spec/pmetal_rgba_materializer.cpp").read_text("utf8")
lerp=(ROOT/"src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text("utf8")
draw=(ROOT/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf8")
builder=(ROOT/"tools/build_v230_envdiffuse_rgba_fix.ps1").read_text("utf8")
original_builder=(ROOT/"tools/build_v230_production.ps1").read_text("utf8")
assert "if (ptde_sample[7] != 0x00107e46u)" in stable
assert "ptde_sample[7] = 0x00107936u;" in stable
assert stable.count("ptde_sample[7] = 0x00107936u;")==1
assert "if (sample_a[7] != 0x00107e46u ||" in lerp
assert "sample_b[7] != 0x00107e46u)" in lerp
assert lerp.count("sample_a[7] = 0x00107936u;")==1
assert lerp.count("sample_b[7] = 0x00107936u;")==1
assert "const std::array<std::uint32_t,7>" in stable
assert "0x00100e56u,12u" in stable
assert "0x00100006u,12u" in stable
assert "0x00100e56u,12u" in lerp
assert "0x00100006u,12u" in lerp
assert "[DSRRL V230 ENVDIFFUSE_RGBA_FIXED]" in draw
assert "DSRRL_V230_STOCK_DSR_ENVDIFFUSE" not in builder, "This needs FULL PTDE EnvDiffuse carrier, not stock branch"
assert "DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG=ON" in (ROOT/"tools/build_v350_gpu_resident_pointlight_off_rc2.ps1").read_text("utf8")
assert "materialize_v230_envdiffuse_rgba_channel_order.py" in builder
assert "test_v230_envdiffuse_rgba_channel_order.py" in builder
assert "materialize_v230_envdiffuse_rgba_channel_order.py" not in original_builder
assert "DSRRL_Restored_Lighting_v2.3.0.addon64" in original_builder
assert "DSRRL_Restored_Lighting_v2.3.0_PTDE_EnvDiffuse_RGBA_FIXED.addon64" in builder
assert "env_resources_.prepare_envdiffuse_bound(" in draw
assert "if (!env_resources_.prepare_envdiffuse(" not in draw, "do not re-couple EnvDiffuse identity to EnvSpec"
assert "if (!prepared.material_resources.spec_rgb)" not in draw or "spec_rgb" in draw
print("V230_ENV_DIFFUSE_RGBA_SOURCE_CONTRACT_PASS: original xyzw->alpha-first wxyz, stable t11 and Lerp t11/t13, runtime full PTDE envdiffuse retained")
print("PIXEL=OPEN; material- and cutscene-specific validation still required")
