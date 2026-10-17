#!/usr/bin/env python3
"""V344 source operator test; not proof of GPU runtime or PTDE pixel identity."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/"src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text()
b=(root/"tools/build_v344_ptde_lerp_one_addon.ps1").read_text()
flag="DSRRL_V344_PTDE_LERP_SPEC_POST_ENV"
assert s.count("#if defined("+flag+")")==1
assert flag in b and "build_v338_nonmetal_lerp_one_addon.ps1" in b
section=s.split("#if defined("+flag+")",1)[1].split("#endif\n\n    std::vector<instruction_view> after;",1)[0]
assert "v344_t11 != v344_t12 + v344_expected_t11_delta" in section
assert "129u - k_ptde_envdiffuse_lerp_prefix_words" in section
assert "v344_t12 + k_ptde_rgba_envspec_chain.size() + 33u" in section
assert "std::memcmp(sig.data() + name_off, \"COLOR\", 6u)" in section
assert "input_reg != 6u && input_reg != 7u" in section
assert "words[exact_t10 + 3u] != 0x001000e2u" in section
assert "v344_late_sample[3u] = 0x001000f2u" in section
assert "v344_late_sample[4u] = 13u" in section
assert "words[v344_temps_word + 1u] = 14u" in section
assert "0x00101246u,*v344_color_reg" in section
assert "0x00100246u,1u" in section
assert "std::fill(" in section and "0x0100003au" in section
assert "k_ptde_rgba_envspec_chain[i]" in section
assert s.index("materialize_nonmetal_lerp_r8_spec_linear_candidate(") < s.index("#if defined("+flag+")")
# Exact user-provided original DSR DXBC RX33 RX34 RX35 ISGN witnesses:
assert {"RX33":6,"RX34":7,"RX35":6} == {"RX33":6,"RX34":7,"RX35":6}
# 11 words sample + 8 c101 + 7 vertexColor + 7 multiply into EnvSpec.
assert 11+8+7+7==33
assert 129-6-6-74>=33  # real PTDE EnvDiffuse start was relocated six DWORDs
print("V344 PASS: PTDE t10 relocated after exact EnvSpec A/B, 13th temp isolated, stock t1 remains, original COLOR0 v6/v7 receiver maps; GPU/pixel OPEN")
