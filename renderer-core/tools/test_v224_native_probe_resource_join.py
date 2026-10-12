#!/usr/bin/env python3
"""V224 source replay and negative routing/identity oracles."""
import importlib.util
from pathlib import Path

path = Path(__file__).with_name("apply_v224_native_probe_resource_join.py")
spec = importlib.util.spec_from_file_location("v224", path)
v224 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(v224)

fixture_h = (
    'class envspec_resource_runtime { public:\n'
    + v224.HEADER_ANCHOR + '\n};\n'
)
fixture_r = (
    '#include <d3d11.h>\n'
    'void init() {};\n' + v224.RESOURCE_ANCHOR +
    '\n    return false;\n}\n'
)
fixture_d = (
    'void prepare() {\n'
    '    bool env_resource_ready = false;\n'
    + v224.DRAW_ANCHOR +
    '\n    if (!env_resource_ready) return;\n}\n'
)
h, r, d = v224.patches(fixture_h, fixture_r, fixture_d)
assert v224.patches(h, r, d) == (h, r, d), "must be idempotent"

# Original P_Metal source/draw remains outside this exact nonmetal opt-in.
assert "sidecar_20261010::exact_nonmetal_draw(material, decision)" in d
assert "if (!env_resource_ready && !k_native_dsr_cubemap_feed" in d
assert d.index("V224 NATIVE RESOURCE JOIN") < d.index(
    v224.DRAW_ANCHOR)
assert d.count("env_resource_ready = recovered;") == 1

# No global resource guesses: a hit is only SHA256-authenticated resource.
assert "g_native_resources.find(observed.resource_key)" in r
assert "g_resource_by_view.find(view_key)" in r
assert "td.Width == k_dsr_size" in r
assert "td.Height == k_dsr_size" in r
assert "td.ArraySize == k_faces" in r
assert "td.MipLevels == k_dsr_levels" in r
assert "D3D11_SRV_DIMENSION_TEXTURECUBE" in r
assert "td.SampleDesc.Count == 1u" in r
assert "D3D11_RESOURCE_MISC_TEXTURECUBE" in r
assert "srv.TextureCube.MostDetailedMip == 0u" in r
assert "DXGI_FORMAT_BC6H_UF16" in r
assert "DXGI_FORMAT_BC6H_SF16" in r
assert "if (!observed.resource_registered)" in r
assert "observed.probe < env::k_legacy_envspec_probe_count" in r
assert "get_ptde_cube(diag.a.probe, slot, ignored)" in r
assert "get_ptde_cube(diag.b.probe, slot, ignored)" in r
assert "g_ptde_cubes.find(key_a)" in r
assert "g_ptde_cubes.find(key_b)" in r
assert "ptde_a->AddRef()" in r and "ptde_b->AddRef()" in r
assert "ptde_sampler->AddRef()" in r
assert "g_native_resources[" not in v224.RESOURCE_ADD
assert "g_resource_by_view[" not in v224.RESOURCE_ADD
assert "on_init_resource(" not in v224.RESOURCE_ADD
assert "#if defined(DSRRL_V224_EXACT_NONMETAL_T12_RESOURCE_JOIN)" in h
assert "prepared.ready = true" in r
assert "pixel=OPEN" in d

for wrong in (
    (fixture_h.replace(v224.HEADER_ANCHOR, 'bad'), fixture_r, fixture_d),
    (fixture_h, fixture_r.replace(v224.RESOURCE_ANCHOR, 'bad'), fixture_d),
    (fixture_h, fixture_r, fixture_d.replace(
        '    bool env_resource_ready = false;', 'const bool env_resource_ready = false;')),
    (fixture_h, fixture_r, fixture_d.replace(v224.DRAW_ANCHOR, 'bad')),
):
    try:
        v224.patches(*wrong)
    except RuntimeError:
        pass
    else:
        raise AssertionError("V224 missing guard accepted")
print("V224_EXACT_NATIVE_T12_SHA_RESOURCE_JOIN_ORACLE_PASS")
print("V224_NO_GUESSED_PROBE_NEGATIVE_GATES_PMETAL_UNCHANGED_PASS")
print("V224_PIXEL_AND_RUNTIME_OPEN")
