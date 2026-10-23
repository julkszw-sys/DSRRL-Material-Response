#!/usr/bin/env python3
"""V348 exact source operator tests; native CI validates PE independently."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/"src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text()
d=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
b=(root/"tools/build_v348_one_addon.ps1").read_text()
f="DSRRL_V348_PTDE_LERP_SPECRGB_RGB_FIX"
assert s.count("#if defined("+f+")") == 2, s.count(f)
assert "v344_late_sample[3u] = 0x001000f2u;" in s
assert "v344_late_sample[4u] = 13u;" in s
assert "v344_late_sample[7u] = 0x00107e46u;" in s
assert "words[exact_t10 + 7u] != 0x00107936u" in s
assert "0x00208006u,12u,0u, // cb12[0].xxxx" in s
assert "0x00208246u,12u,0u, // V347 cb12[0].xyz" in s
assert 'v344_t11 != v344_t12 + v344_expected_t11_delta' in s
assert 'k_ptde_rgba_envspec_chain[i]' in s
assert "DSRRL_V345_UNIFIED_HEMENVLERP" in b and f in b
assert "[DSRRL V348 SPEC RGB]" in d
assert "lerp_nonmetal_r8_replacements_" in d # only behind disabled V345 branch
def swizzle(word):
    return tuple((word >> (4+2*i))&3 for i in range(4))
assert swizzle(0x00107936)==(3,0,1,2)
assert swizzle(0x00107e46)==(0,1,2,3)
assert swizzle(0x00208246)==(0,1,2,0)
assert swizzle(0x00208006)==(0,0,0,0)
rgba=(.12,.34,.92,.06)
old=[rgba[x] for x in swizzle(0x00107936)[:3]]
new=[rgba[x] for x in swizzle(0x00107e46)[:3]]
assert old==[.06,.12,.34] and new==[.12,.34,.92]
print("V348 SOURCE PASS: one RX-specific HemEnvLerp shader per RX33..35, t10 full XYZW post EnvSpec, CB12 scalar c101.x broadcast; runtime/pixel OPEN")
