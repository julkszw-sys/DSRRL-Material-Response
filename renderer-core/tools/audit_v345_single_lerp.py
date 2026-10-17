#!/usr/bin/env python3
"""V345 single canonical DX11 Lerp shader source/gating regression."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
draw=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
mat=(root/"src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text()
build=(root/"tools/build_v345_single_lerp_one_addon.ps1").read_text()
v344=mat.split("#if defined(DSRRL_V344_PTDE_LERP_SPEC_POST_ENV)",1)[1].split("#endif",1)[0]
section=draw.split("register_lerp_replacement(",1)[1].split("register_lerp_nonmetal_r8_candidate(",1)[0]
single=section.split("#if defined(DSRRL_V345_UNIFIED_HEMENVLERP)",1)[1].split("const auto payload_sha256",1)[0]
assert "materialize_nonmetal_lerp_r8_spec_linear_candidate(" in single
assert "unified.empty()" in single
assert "dxbc=unified.data();" in single
assert "dxbc_size=unified.size();" in single
assert "lerp_replacements_" in section
assert "candidate=REJECTED" in single
assert "candidate=CREATED" in section
skip=draw.split("register_lerp_nonmetal_r8_candidate(",1)[1].split("bool pmetal_envspec_draw_runtime::prepare(",1)[0]
assert "return true;" in skip.split("#if defined(DSRRL_V345_UNIFIED_HEMENVLERP)",1)[1].split("#else",1)[0]
assert "lerp_nonmetal_r8_replacements_" not in skip.split("#if defined(DSRRL_V345_UNIFIED_HEMENVLERP)",1)[1].split("#else",1)[0]
route=draw.split("bool pmetal_envspec_draw_runtime::prepare(",1)[1]
assert "#if defined(DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB) && !defined(DSRRL_V345_UNIFIED_HEMENVLERP)" in route
assert "stage=request_ready" in route and "single_ps=1" in route
assert "pmetal ? \"P_METAL\" : \"NONMETAL\"" in route
assert "r8_selected" in route  # remains only behind disabled alternative gate
assert "v344_t11 != v344_t12 + 129u" in v344
assert "v344_late_sample[4u] = 13u" in v344
assert "words[v344_temps_word + 1u] = 14u" in v344
assert "std::memcmp(sig.data() + name_off, \"COLOR\", 6u)" in v344
assert "v344_modulate" in v344
assert "c101_mul_word + k_c101_mul_words" in v344
assert "/DDSRRL_V344_PTDE_LERP_SPEC_POST_ENV" in build
assert "/DDSRRL_V345_UNIFIED_HEMENVLERP" in build
assert "build_v338_nonmetal_lerp_one_addon.ps1" in build
print("PASS V345 canonical single RX33/34/35 HemEnvLerp: no secondary R8 program, PTDE t10 post-EnvSpec, P_Metal/nonmetal share same PS; activation/pixel OPEN")
