#!/usr/bin/env python3
"""Static V326 source + original RX33..35 token regression.

These are owner-authenticated stock DSR SAMPLE token witnesses, not final R19
or R8 materialized DXBC and not an in-game/pixel verification.
"""
from pathlib import Path

cpp = (
    Path(__file__).resolve().parents[1]
    / "src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp"
).read_text(encoding="utf-8")
marker = "bool materialize_nonmetal_lerp_r8_spec_linear_candidate("
assert cpp.count(marker) == 1, "exact nonmetal R8 function identity drift"
scope = cpp.split(marker, 1)[1]
assert scope.count("words[exact_t10 + 7u] = 0x00107e46u;") == 1
assert "words[exact_t10 + 7u] != 0x00107936u" in scope
assert "words[exact_t10 + 3u] != 0x001000e2u" in scope
assert "native_t1_count != 1u" in scope
assert "exact_t10 == no_t10" in scope
assert "words[sample.offset + 8u]" in scope
assert "words[gamma_exp]=(words[gamma_exp]&~0x7ffu)|0x36u;" in scope
assert "return legacy_plan::dxbc::checksum_container_valid(" in scope

# Exact 11-DWORD SAMPLEs from the original RX33/34/35 Lerp DSR DXBC.
# rN.xyzw <- SAMPLE(vM, t1.wxyz, s1).
native_samples = {
    33: [0x8b000045,0x800000c2,0x00155543,0x001000f2,7,
         0x00101046,7,0x00107936,1,0x00106000,1],
    34: [0x8b000045,0x800000c2,0x00155543,0x001000f2,6,
         0x00101046,8,0x00107936,1,0x00106000,1],
    35: [0x8b000045,0x800000c2,0x00155543,0x001000f2,5,
         0x00101046,7,0x00107936,1,0x00106000,1],
}
def rgb_select(swizzle_word, rgba):
    return tuple(rgba[(swizzle_word >> (4 + 2 * lane)) & 3] for lane in range(3))

for rx, native in native_samples.items():
    stock = native.copy()
    r19 = native.copy()
    r19[3] -= 0x10   # original consumer duplicates stock t1 with RGB dst
    r19[8] = 10      # t10 PTDE sidecar, still stock DSR wxyz
    r8 = r19.copy()
    assert r8[7] == 0x00107936
    r8[7] = 0x00107e46  # V326 only change to duplicated t10 sample
    assert [n for n, (a, b) in enumerate(zip(r19, r8)) if a != b] == [7], rx
    assert stock == native and native[8] == 1 and native[7] == 0x00107936
    assert r8[8] == 10 and r8[3] == 0x001000e2
    test_rgba = (0.21, 0.43, 0.68, 0.92)
    assert rgb_select(r19[7], test_rgba) == (0.92, 0.21, 0.43)
    assert rgb_select(r8[7], test_rgba) == (0.21, 0.43, 0.68)

print("V326 PASS: 3/3 original RX sample tokens; isolated nonmetal R8 t10 RGB identity; source fail-open guards; P_Metal R19 unchanged by this generator")
