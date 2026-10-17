#!/usr/bin/env python3
"""V342 exact DXBC late material-tail diagnostic source proof, not pixel pass.

Positive original stock witnesses (DSR RX33/34/35 HemEnvLerp) carry the unique
late 5-op Schlick sequence. This test verifies its exact shapes and ensures
the patch is enabled only inside the authenticated nonmetal R8-only path.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
shader=(r/"src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text()
runtime=(r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
cmake=(r/"integrated/CMakeLists.txt").read_text()
build=(r/"tools/build_v342_ptde_legacy_lerp_one_addon.ps1").read_text()
flag="DSRRL_V342_PTDE_LEGACY_LERP_MATERIAL_TAIL_DIAG"
assert shader.count("#if defined("+flag+")")==1
assert runtime.count("#if defined("+flag+")")==1
assert "if("+flag+")" in cmake
assert "-D"+flag+"=ON" in build
assert "-DDSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=ON" in build
assert "materialize_nonmetal_lerp_r8_spec_linear_candidate(" in shader
s=shader.split("bool materialize_nonmetal_lerp_r8_spec_linear_candidate(",1)[1]
assert s.index("words[v342_mad_word + 5u]") > s.index("words[c101_mul_word + 5u]")
assert s.count("words[v342_mad_word + 5u] = 0x00004001u;")==1
assert s.count("words[v342_mad_word + 6u] = 0x00000000u;")==1
assert s.count("v342_matches != 1u")==1
assert "0xc0b1c059u" in s and "0xc0df760cu" in s
assert "words[fit.offset + 7u] != k_fit_a" in s
assert "words[fit.offset + 8u] != k_fit_b" in s
assert "words[add.offset + 6u] != words[ex.offset + 2u]" in s
assert "late_DSR_schlick_addend=0" in runtime
assert "if (r8_selected &&" in runtime
assert "material.route_index != 345u" in runtime
assert "lerp_replacements_" in runtime and "lerp_nonmetal_r8_replacements_" in runtime
assert "source.beta" in runtime
assert "-DDSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE=ON" in build
# Exact original DXBC late sequence witnesses, from original user archives:
# (receiver, fit MAD, Schlick multiply, EXP2, live-out MAD, swizzle)
witnesses=[
 (33,2322,2331,2338,2343,0x00100556),
 (34,2241,2250,2257,2262,0x00100ff6),
 (35,1894,1903,1910,1915,0x00100ff6),
]
for rx,fit,mul,exp,tail,src_token in witnesses:
    assert mul-fit==9 and exp-mul==7 and tail-exp==5
    assert src_token in (0x00100556,0x00100ff6)
# Replacement preserves instruction lengths and source0, source2, dst:
# source1 r7/r8.(x/...) -> float 0.0 with same 2-DWORD source length.
# IMPORTANT: RGB output, PointLight, shadow, original source beta, and
# nonmetal Stable remain outside this candidate; still not pixel-equivalent.
print("PASS V342: nonmetal R8 only, exact 5-op late Schlick signature and zero scalar addend, 3 receivers, controls preserved by routing; runtime/pixels OPEN")
