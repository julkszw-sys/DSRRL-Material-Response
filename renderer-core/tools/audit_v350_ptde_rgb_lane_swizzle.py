#!/usr/bin/env python3
"""V350: DXBC source swizzle is indexed by ABSOLUTE destination channel.
Check original PTDE YZW liveout and exact RGB component mapping on RX33-35.
This is a bytecode/source test, not PTDE pixel equivalence.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/"src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text()
d=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
b=(root/"tools/build_v350_one_addon.ps1").read_text()
needle="0x07000038u,0x001000e2u,1u,0x00100e46u,1u,\n        0x00100906u,13u"
assert s.count(needle)==1
assert "DSRRL_V350_PTDE_LERP_RGB_COMPONENT_SWIZZLE" in s
assert "DSRRL_V349_PTDE_LERP_ENVSPEC_YZW_FIX" in b
assert "DSRRL_V348_PTDE_LERP_SPECRGB_RGB_FIX" in b
assert "DSRRL_V350_PTDE_LERP_RGB_COMPONENT_SWIZZLE" in b
assert "[DSRRL V350 PTDE LERP RGB]" in d
assert "materialize_nonmetal_lerp_r8_spec_linear_candidate" in d
assert "v344_t11 != v344_t12 + v344_expected_t11_delta" in s
assert "v344_late_sample[7u] = 0x00107e46u" in s
assert "0x00208006u,12u,0u, // cb12[0].xxxx" in s
def swz(token):
    return tuple((token >> (4+2*c))&3 for c in range(4))
def mask(token):
    return (token >> 4)&15
dst=0x001000e2
a=0x00100e46
v=0x00100906
assert mask(dst)==0xE
assert swz(a)==(0,1,2,3)
assert swz(v)==(0,0,1,2)
assert [swz(a)[lane] for lane in (1,2,3)]==[1,2,3]
assert [swz(v)[lane] for lane in (1,2,3)]==[0,1,2]
# V349 was effectively z,w,y * y,z,x on YZW, demonstrably wrong.
assert [swz(0x00100796)[lane] for lane in (1,2,3)]==[2,3,1]
assert [swz(0x00100246)[lane] for lane in (1,2,3)]==[1,2,0]
# Colored PTDE EnvSpec plus differently colored SpecRGB.
env=[0.4,0.21,0.57,0.83]  # X is separate, YZW carry PTDE RGB
spc=[0.18,0.73,0.46]
out=env.copy()
for lane in (1,2,3):
    out[lane] = env[swz(a)[lane]]*spc[swz(v)[lane]]
assert out[0]==env[0]
assert all(abs(out[i+1]-env[i+1]*spc[i])<1e-12 for i in range(3))
print("V350 exact masked YZW source lanes PASS for all RX33/34/35; P_Metal and nonmetal share one PS. Runtime and PTDE pixels OPEN.")
