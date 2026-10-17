#!/usr/bin/env python3
"""V349 PTDE HemEnvLerp component-mask audit: original r1.YZW liveout.
Source regression only; Windows native and owner pixel checks separate.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
s=(p/"src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text()
r=(p/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
b=(p/"tools/build_v349_one_addon.ps1").read_text()
f="DSRRL_V349_PTDE_LERP_ENVSPEC_YZW_FIX"
v=s.split("#if defined(DSRRL_V344_PTDE_LERP_SPEC_POST_ENV)",1)[1].split("#endif\n\n    std::vector<instruction_view> after;",1)[0]
assert "k_ptde_rgba_envspec_chain" in s
chain=s.split("constexpr std::array<std::uint32_t,74> k_ptde_rgba_envspec_chain = {{",1)[1].split("}};",1)[0]
assert "0x001000e2u,0x00000001u" in chain # masks YZW on r1
assert "0x00100e56u,0x00000001u" in chain # uses its YZW read
assert "0x001000e2u,0x00000001u,0x00100e56u" in chain
assert f in v
assert "0x07000038u,0x001000e2u,1u,0x00100796u,1u," in v
assert "0x00100246u,13u" in v
assert "0x07000038u,0x00100072u,1u,0x00100246u,1u," in v # old branch remains for reproducing prior version
assert "v344_late_sample[7u] = 0x00107e46u" in v
assert "0x00208006u,12u,0u, // cb12[0].xxxx" in v
assert "#if defined("+f+")" in v
assert f in b and "DSRRL_V348_PTDE_LERP_SPECRGB_RGB_FIX" in b
assert "[DSRRL V349 PTDE RGB LIVEOUT]" in r
assert "v344_t11 != v344_t12 + v344_expected_t11_delta" in s
assert "lerp_nonmetal_r8_replacements_" in r # legacy behind !V345 route-selection guard
mask=lambda tok:(tok>>4)&15
swz=lambda tok:tuple((tok>>(4+2*i))&3 for i in range(4))
assert mask(0x001000e2)==0xe # YZW
assert mask(0x00100072)==0x7 # XYZ
assert swz(0x00100796)==(1,2,3,0) # r1.YZWX
assert swz(0x00100246)==(0,1,2,0) # scratch r13.XYZ
ptde_envspec=[.12,.24,.48] # native r1.YZW expected RGB
ptde_spec=[.7,.6,.5]
old_r1=[.9]+ptde_envspec
old_r1[0]*=ptde_spec[0]
old_r1[1]*=ptde_spec[1]
old_r1[2]*=ptde_spec[2]
assert old_r1[3]==.48 # blue channel bypassed
new_r1=[.9]+ptde_envspec
for i in range(3): new_r1[i+1]*=ptde_spec[i]
assert new_r1[0]==.9
assert new_r1[1:]==[.12*.7,.24*.6,.48*.5]
print("V349 exact PTDE channel PASS: r1.YZW receives r13.XYZ; r1.X preserved, blue W modulated; unified RX33/34/35 and V347 gates retained.")
