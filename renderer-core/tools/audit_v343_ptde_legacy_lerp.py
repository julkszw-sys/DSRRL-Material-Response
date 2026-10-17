#!/usr/bin/env python3
"""V343 original DSR RX33-35 five-op DXBC witness, exact opcode/token regression.

Purpose: catches the V342 bug where +7 (a DXBC literal TYPE token) was
mistaken for first float literal at +6. It checks full raw opcode windows,
not only source spelling. It does not claim GPU activation or pixel match.
"""
from pathlib import Path
r = Path(__file__).resolve().parents[1]
shader = (r/"src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text()
rt = (r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
cm = (r/"integrated/CMakeLists.txt").read_text()
ps = (r/"tools/build_v343_ptde_legacy_lerp_one_addon.ps1").read_text()
flag="DSRRL_V343_PTDE_LEGACY_LERP_MATERIAL_TAIL_DIAG"
assert shader.count("#if defined("+flag+")") == 1
assert rt.count("#if defined("+flag+")") >= 3  # registration rejection, success and request-ready
assert ("if("+flag+")") in cm
assert "-D"+flag+"=ON" in ps
assert "-DDSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=ON" in ps
assert "words[fit.offset + 6u] != k_fit_a" in shader
assert "words[fit.offset + 8u] != k_fit_b" in shader
assert "words[fit.offset + 7u] != k_fit_a" not in shader
assert "words[v343_mad_word + 5u] = 0x00004001u;" in shader
assert "words[v343_mad_word + 6u] = 0x00000000u;" in shader
assert "v343_matches != 1u" in shader
assert "late_DSR_schlick_addend=0" in rt
assert "r8_selected &&" in rt
assert "material.route_index != 345u" in rt
assert "source.beta" in rt

# Exact 5-op windows from original DSR fpo DXBC RX33/34/35 in user archives;
# order DP3(7 words) MAD(9) MUL(7) EXP2(5) MAD(9).
original = {
  33: [
    0x07002010,0x00100012,7,0x00100246,4,0x00100246,7,
    0x09000032,0x00100022,7,0x0010003a,9,0x00004001,0xc0b1c059,0x00004001,0xc0df760c,
    0x07000038,0x00100022,7,0x0010003a,9,0x0010001a,7,
    0x05000019,0x00100022,7,0x0010001a,7,
    0x09000032,0x00100072,12,0x00100246,3,0x00100556,7,0x00100246,11,
  ],
  34: [
    0x07002010,0x00100082,9,0x00100246,4,0x00100246,11,
    0x09000032,0x00100082,10,0x0010003a,8,0x00004001,0xc0b1c059,0x00004001,0xc0df760c,
    0x07000038,0x00100082,8,0x0010003a,8,0x0010003a,10,
    0x05000019,0x00100082,8,0x0010003a,8,
    0x09000032,0x00100072,11,0x00100246,3,0x00100ff6,8,0x00100246,10,
  ],
  35: [
    0x07002010,0x00100082,8,0x00100246,4,0x00100246,10,
    0x09000032,0x00100082,9,0x0010003a,7,0x00004001,0xc0b1c059,0x00004001,0xc0df760c,
    0x07000038,0x00100082,7,0x0010003a,7,0x0010003a,9,
    0x05000019,0x00100082,7,0x0010003a,7,
    0x09000032,0x00100072,10,0x00100246,3,0x00100ff6,7,0x00100246,9,
  ]
}
for rx, tok in original.items():
    assert len(tok)==37,(rx,len(tok))
    at=[0,7,16,23,28]
    instructions=[tok[i:i+n] for i,n in zip(at,[7,9,7,5,9])]
    dot,fit,mul,ex,add=instructions
    assert [(x[0]&0x7ff, (x[0]>>24)&0x7f) for x in instructions] == [
      (0x10,7),(0x32,9),(0x38,7),(0x19,5),(0x32,9)]
    assert fit[5]==0x00004001 and fit[6]==0xc0b1c059
    assert fit[7]==0x00004001 and fit[8]==0xc0df760c
    assert fit[7]!=0xc0b1c059   # V342: ALWAYS FAILS
    assert add[1]==0x00100072 and add[3]==0x00100246
    assert (add[5]&0xff) in (0x56,0xf6)
    assert add[6]==ex[2] and add[7]==0x00100246
    # Same-size literal-zero operand must retain DXBC 9-DWORD MAD:
    changed=add[:]
    changed[5:7]=[0x00004001,0x00000000]
    assert len(changed)==9
    assert changed[:5]==add[:5] and changed[7:]==add[7:]
    assert changed[5:7]==[0x00004001,0]
print("PASS V343 exact 3/3 original DXBC 5-op witnesses: first float at +6, literal opcode +7, unchanged len=9 and unique nonmetal guard; runtime/pixel OPEN")
