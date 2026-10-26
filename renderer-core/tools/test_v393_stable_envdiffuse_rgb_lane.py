#!/usr/bin/env python3
"""Operator-local DXBC swizzle regression audit (not a PTDE pixel PASS)."""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
s=(root/"src/operators/env_spec/pmetal_rgba_materializer.cpp").read_text(encoding="utf-8")
for name,next_name in [
    ("apply_exact_ptde_envdiffuse_consumer(", "apply_exact_ptde_envdiffuse_consumer_isolated("),
    ("apply_exact_ptde_envdiffuse_consumer_isolated(", "bool remove_dsr_postmerge_visibility_scalar_r3("),
]:
    start=s.index("bool "+name)
    end=s.find(next_name,start+20)
    if end<0:raise AssertionError("function boundary not found: "+name)
    f=s[start:end]
    begin=f.index("apply_ptde_endpoint{{")
    endpt=f[begin:begin+220]
    assert "0x00100f96u" in endpt,(name,"wrong final RGB swizzle")
    assert "0x00100e56u" not in endpt,(name,"YYZ regression")
    div=f[f.index("decode_rgb_over_alpha{{"):begin]
    assert "0x001000e2u" in div and "0x00100e56u" in div and "0x00100006u" in div, "alpha-first DIV must stay intact"

def channels(token):
    assert token&3==2 and (token>>2)&3==1
    return tuple((token>>(4+2*i))&3 for i in range(4))

assert channels(0x00100e56)==(1,1,2,3)
assert channels(0x00100f96)==(1,2,3,3)

def out(packed,gain,token):
    # Packed sample is WXYZ/alpha-first, as certified in the V230 donor contract.
    decoded=[packed[1]/packed[0],packed[2]/packed[0],packed[3]/packed[0]]
    regs=[packed[0]]+decoded
    return tuple(regs[c]*gain[i] for i,c in enumerate(channels(token)[:3]))

for packed in [(0.7,0.3,0.5,0.6),(1.0,0.15,0.27,0.4)]:
    for gain in [(5.0,5.0,5.0),(1.5,1.32352948,0.911764741)]:
        expected=tuple(packed[i+1]/packed[0]*gain[i] for i in range(3))
        actual=out(packed,gain,0x00100f96)
        assert all(abs(x-y)<1e-8 for x,y in zip(actual,expected))
        assert any(abs(x-y)>1e-6 for x,y in zip(out(packed,gain,0x00100e56),expected))
print("V393 STABLE exact RGB YZW final lane audit PASS; runtime/pixel OPEN")
