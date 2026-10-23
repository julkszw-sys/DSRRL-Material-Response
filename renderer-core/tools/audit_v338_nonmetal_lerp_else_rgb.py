#!/usr/bin/env python3
"""V338 nonmetal HemEnvLerp PTDE SpecRGB YZW carrier contract.

In stock DSR t1 sample, source swizzle wxyz into rSpec.xyzw makes
rSpec.yzw contain RGB. R19's PTDE t10 writes destination YZW only.
The V326 source swizzle xyzw was incorrect for that destination mask:
it wrote G,B,A. V338 retains t10 source wxyz and rehomes raw-c101 MUL
from XYZ to YZW, preserving rSpec.x stock native alpha/material lane.

No game assets needed for normal CI; optional --shader-dir checks
original unmodified DSR DXBC. This is source/operator verification only.
"""
import argparse
import pathlib
import struct

SITES = {
    "FRPG_Phn_DifSpcBmp______Csd_HemEnvLerp.fpo.dxbc": (7, 1081, 1292, 1322),
    "FRPG_Phn_DifSpcBmp______Sdw_HemEnvLerp.fpo.dxbc": (6, 990, 1201, 1231),
    "FRPG_Phn_DifSpcBmp__________HemEnvLerp.fpo.dxbc": (5, 650, 861, 891),
}

def words_from_dxbc(data):
    assert data[:4] == b"DXBC", "not DXBC"
    num = struct.unpack_from("<I", data, 28)[0]
    for i in range(num):
        off = struct.unpack_from("<I", data, 32 + i * 4)[0]
        tag, size = struct.unpack_from("<4sI", data, off)
        if tag in (b"SHEX", b"SHDR"):
            payload = data[off + 8:off + 8 + size]
            return struct.unpack("<%dI" % (len(payload) // 4), payload)
    raise AssertionError("DXBC lacks SHEX/SHDR")

def verify_originals(shader_dir):
    checked = 0
    for basename, (spec_reg, if_word, else_word, mul_word) in SITES.items():
        w = words_from_dxbc((shader_dir / basename).read_bytes())
        assert w[if_word:if_word+4] == (
            0x0400001f, 0x0020800a, 0, 85), basename + " cb85"
        assert w[else_word] == 0x01000012, basename + " ELSE"
        assert w[mul_word:mul_word+8] == (
            0x08002038, 0x00100072, 1, 0x00100796, spec_reg,
            0x00208246, 0, 10), basename + " native YZW read"
        checked += 1
    return checked

def verify_mapping():
    src = (0.21, 0.42, 0.63, 0.84)  # RGBA sidecar
    native_alpha = 0.35
    raw_c101 = 1.5
    # Native t1 wxyz writes A,R,G,B into XYZW.
    stock_reg = (native_alpha, *src[:3])
    assert stock_reg[1:] == src[:3]
    # V326 PTDE t10 identity xyzw writes only YZW -> G,B,A.
    wrong_reg = (stock_reg[0], *src[1:])
    assert wrong_reg[1:] != src[:3]
    # V338 original wxyz t10 writes PTDE RGB to YZW; raw-c101
    # multiplies only YZW, and leaves stock alpha at X untouched.
    fixed_reg = (stock_reg[0], *(x * raw_c101 for x in src[:3]))
    assert fixed_reg[0] == native_alpha
    assert fixed_reg[1:] == tuple(x * raw_c101 for x in src[:3])

def verify_source(source):
    assert "if (depth != 0u || in_else)" in source  # THEN metal still isolated
    assert "words[exact_t10 + 7u] != 0x00107936u" in source
    assert "words[exact_t10 + 7u] = 0x00107e46u;" not in source
    assert "words[c101_mul_word + 1u] = 0x001000e2u;" in source
    assert "words[c101_mul_word + 3u] = 0x00100796u;" in source
    assert "words[c101_mul_word + 5u] = 0x00208006u;" in source
    assert "words[c101_mul_word + 2u] != ptde_rgb_reg" in source
    assert "words[cur.offset + 3u] != 0x00100796u" in source
    assert "else_rgb_reads != 1u" in source
    assert "words[else_rgb_word + 3u] = 0x00100246u;" not in source

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--source", type=pathlib.Path,
                   default=pathlib.Path(__file__).resolve().parents[1] /
                   "src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
    p.add_argument("--shader-dir", type=pathlib.Path)
    args = p.parse_args()
    verify_mapping()
    verify_source(args.source.read_text(encoding="utf-8"))
    n = verify_originals(args.shader_dir) if args.shader_dir else 0
    print("V338 RGB carrier and C101 source guards PASS; original DXBC:",
          str(n) + "/3" if args.shader_dir else "optional")

if __name__ == "__main__":
    main()
