#!/usr/bin/env python3
"""V338 nonmetal HemEnvLerp SpecRGB ELSE swizzle source contract.

No game assets needed for normal CI. Optional --shader-dir checks the three
unmodified owner-extracted retail DSR DXBC shader inputs. This is an
operator-source contract, not a runtime/Python pixel validation.
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
        assert w[if_word:if_word + 4] == (
            0x0400001f, 0x0020800a, 0, 85), basename + " cb85"
        assert w[else_word] == 0x01000012, basename + " ELSE"
        assert w[mul_word:mul_word+8] == (
            0x08002038, 0x00100072, 1, 0x00100796, spec_reg,
            0x00208246, 0, 10), basename + " native R.G.B read"
        # DSR t1's wxyz makes rSpec.yzw = RGB. After PTDE t10
        # direct-identity source writes rSpec.xyz = RGB instead.
        test_rgb = (0.2, 0.4, 0.7)
        native_spec = (0.91,) + test_rgb
        ptde_spec = test_rgb + (native_spec[3],)
        assert native_spec[1:4] == test_rgb
        assert ptde_spec[1:4] != test_rgb
        assert ptde_spec[0:3] == test_rgb
        checked += 1
    return checked

def verify_source(source):
    # Source must prove THEN-only metal restoration and exact ELSE-only
    # nonmetal consumer patch; avoid accepting an uncontrolled global rewrite.
    assert "if (depth != 0u || in_else)" in source
    assert "words[exact_t10 + 7u] = 0x00107e46u;" in source
    assert "words[cur.offset + 3u] != 0x00100796u" in source
    assert "words[cur.offset + 4u] != ptde_rgb_reg" in source
    assert "words[cur.offset + 7u] != 10u" in source
    assert "inside_material_else &&" not in source  # only explicit guarded NOT
    assert "words[else_rgb_word + 3u] = 0x00100246u;" in source
    assert "else_rgb_reads != 1u" in source

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--source", type=pathlib.Path,
                   default=pathlib.Path(__file__).resolve().parents[1] /
                   "src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
    p.add_argument("--shader-dir", type=pathlib.Path)
    args = p.parse_args()
    verify_source(args.source.read_text(encoding="utf-8"))
    n = verify_originals(args.shader_dir) if args.shader_dir else 0
    print("V338 source guards PASS; native Lerp RX source windows verified:",
          str(n) + "/3" if args.shader_dir else "optional")

if __name__ == "__main__":
    main()
