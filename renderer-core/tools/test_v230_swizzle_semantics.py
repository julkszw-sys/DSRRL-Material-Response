#!/usr/bin/env python3
"""D3D11 DXBC operand encoding and RGB/A regression contracts.

This checks actual encoded operand selections and numeric forward equations,
not merely names of channels in comments. GPU/PTDE pixel equality is separate.
"""
from pathlib import Path
from math import isclose

XYZW="xyzw"
def decode(token):
    assert token & 3 == 2,hex(token)
    mode=(token>>2)&3
    if mode==0: return "mask:"+''.join(XYZW[i] for i in range(4) if token&(1<<(4+i)))
    if mode==1: return ''.join(XYZW[(token>>(4+2*i))&3] for i in range(4))
    if mode==2: return "select:"+XYZW[(token>>4)&3]
    raise AssertionError(hex(token))

expected={
    0x001000f2:"mask:xyzw",
    0x001000e2:"mask:yzw",
    0x00100e56:"yyzw",
    0x00100006:"xxxx",
    0x00100072:"mask:xyz",
    0x00100f96:"yzww",
    0x00208246:"xyzx",
    0x00107936:"wxyz",
}
for token,want in expected.items():
    got=decode(token)
    assert got==want,(hex(token),got,want)

def swizzle(values,token):
    return tuple(values[XYZW.index(ch)] for ch in decode(token))
def forward(sample,endpoint,gather):
    # t11 source swizzle .wxyz => physical scratch [A,R,G,B]
    r=list(swizzle(sample,0x00107936))
    assert r[0]!=0
    numerator=swizzle(r,0x00100e56)
    denominator=swizzle(r,0x00100006)
    for lane in (1,2,3):
        r[lane]=numerator[lane]/denominator[lane]
    color=swizzle(r,gather)
    return tuple(color[i]*endpoint[i] for i in range(3))

for raw,bank in [
    ((0.2,0.3,0.5,0.8),(1.2,0.8,0.6)),
    ((0.05,0.45,0.7,0.5),(1.,1.,1.)),
    ((0.5,0.1,0.2,0.25),(0.25,0.8,1.7))
]:
    reference=tuple(raw[i]/raw[3]*bank[i] for i in range(3))
    result=forward(raw,bank,0x00100f96)
    buggy=forward(raw,bank,0x00100e56)
    assert all(isclose(a,b,rel_tol=1.e-12) for a,b in zip(result,reference))
    assert any(not isclose(a,b,rel_tol=1.e-12) for a,b in zip(buggy,reference))

overlay=Path("renderer-core/tools/materialize_v230_envdiffuse_swizzle_guard.py").read_text("utf-8")
assert "ENVDIFFUSE_POST_OPERANDS_DRIFT" in overlay
assert "ENVDIFFUSE_T11_RESOURCE_SWIZZLE_DRIFT" in overlay
assert "0x00100f96u" in overlay
assert "0x00107936u" in overlay
print("V230_SWIZZLE_SEMANTICS_PASS 8 operand encodings 3 numeric cases previous-bug rejected")
