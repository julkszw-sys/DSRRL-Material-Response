#!/usr/bin/env python3
"""DSRRL PTDE EnvSpec offline decoder.

Input contract:
  PTDE_GI_ENVSPEC_PACK_RGBA.bin
  342 probes * 4 slots * 32*32*6 texels * RGBA8 = 33,619,968 bytes

Transform per texel:
  radiance.rgb = (R/A, G/A, B/A) using normalized channels,
               = (R_byte/A_byte, G_byte/A_byte, B_byte/A_byte)
  output = DXGI_FORMAT_R11G11B10_FLOAT packed uint32

No gamma/pow, gain, exposure, clamping to [0,1], or runtime alpha decode is performed.
"""
from __future__ import annotations
import argparse, hashlib, math, os, struct, sys
from pathlib import Path

INPUT_SIZE = 33_619_968
EXPECTED_INPUT_SHA256 = 'c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3'
EXPECTED_REFERENCE_OUTPUT_SHA256 = '6c660c9256e2b1279e53bfc0c3c8663ed39cab130ebba238fb7fb22c4f223e4b'


def f32(x: float) -> float:
    return struct.unpack('<f', struct.pack('<f', x))[0]


def pack_ufloat(x: float, mant_bits: int) -> int:
    """Pack finite non-negative float into E5M6/E5M5 unsigned float.
    Round to nearest, ties to even. Inputs here are <=255, so no overflow.
    """
    x = f32(x)
    if not (x > 0.0):
        return 0
    if math.isinf(x):
        return 0x1F << mant_bits
    if math.isnan(x):
        return (0x1F << mant_bits) | (1 << (mant_bits - 1))

    m, e = math.frexp(x)  # x=m*2**e, 0.5<=m<1
    E = e - 1
    te = E + 15
    if te <= 0:
        # subnormal target, value = mant * 2^(1-bias-mant_bits)
        q = round(x * (2.0 ** (14 + mant_bits)))
        if q <= 0:
            return 0
        if q >= (1 << mant_bits):
            return 1 << mant_bits  # minimum normal (exp=1, mant=0)
        return int(q)
    if te >= 31:
        return 0x1F << mant_bits

    frac = math.ldexp(x, -E) - 1.0
    q = int(round(frac * (1 << mant_bits)))
    if q == (1 << mant_bits):
        te += 1
        q = 0
        if te >= 31:
            return 0x1F << mant_bits
    return (int(te) << mant_bits) | q


def pack_r11g11b10(r: float, g: float, b: float) -> int:
    return pack_ufloat(r,6) | (pack_ufloat(g,6) << 11) | (pack_ufloat(b,5) << 22)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument('input', type=Path)
    ap.add_argument('output', type=Path)
    ap.add_argument('--allow-uncertified-input', action='store_true', help='allow input SHA other than certified retail pack')
    ap.add_argument('--require-reference-output-sha', action='store_true', help='fail unless output matches previously materialized reference R11F pack')
    ns=ap.parse_args()

    raw=ns.input.read_bytes()
    if len(raw)!=INPUT_SIZE:
        raise SystemExit(f'REFUSE: input size {len(raw)} != certified {INPUT_SIZE}')
    insha=sha256_bytes(raw)
    print('input sha256 :',insha)
    if insha!=EXPECTED_INPUT_SHA256 and not ns.allow_uncertified_input:
        raise SystemExit('REFUSE: input SHA is not the certified PTDE PackedGI RGBA corpus')

    out=bytearray(len(raw))
    zero_alpha=0
    max_radiance=0.0
    # Input is already production pack RGBA order (retail DDS BGRA->RGBA was done upstream).
    for i in range(0,len(raw),4):
        R,G,B,A=raw[i:i+4]
        if A==0:
            zero_alpha += 1
            # PTDE RGB/A is undefined here; refuse after scan rather than inventing a value.
            continue
        r=f32(R / A); g=f32(G / A); b=f32(B / A)
        max_radiance=max(max_radiance,r,g,b)
        struct.pack_into('<I',out,i,pack_r11g11b10(r,g,b))

    if zero_alpha:
        raise SystemExit(f'REFUSE: {zero_alpha} texels have alpha=0; no guessed divide-by-zero policy applied')

    outsha=sha256_bytes(out)
    print('output sha256:',outsha)
    print('max RGB/A    :',max_radiance)
    print('reference sha:',EXPECTED_REFERENCE_OUTPUT_SHA256)
    if ns.require_reference_output_sha and outsha!=EXPECTED_REFERENCE_OUTPUT_SHA256:
        raise SystemExit('REFUSE: output differs from the previously materialized R11F reference pack')

    ns.output.parent.mkdir(parents=True,exist_ok=True)
    tmp=ns.output.with_suffix(ns.output.suffix+'.tmp')
    tmp.write_bytes(out)
    os.replace(tmp,ns.output)
    return 0

if __name__=='__main__':
    raise SystemExit(main())
