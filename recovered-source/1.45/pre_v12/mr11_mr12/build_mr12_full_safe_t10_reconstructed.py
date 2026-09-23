#!/usr/bin/env python3
from pathlib import Path
import hashlib

LABEL='MR1.1 FULL_SAFE -> MR1.2 FULL_SAFE internal t10'
EXPECTED_SOURCE_SHA='bb23eefb0d4a1c48330c6001d1c3108c1a0610a798e31a1caf4b37c7f0d325e4'
EXPECTED_TARGET_SHA='fea208ab11baf075d1d99f17771938384090a8102c36096e2d85d86cab2cf1f5'
PATCHES=[
    (0x59e7, bytes.fromhex('488b034533c9'), bytes.fromhex('e8144c100090')),
    (0x5a6a, bytes.fromhex('488b034533c9'), bytes.fromhex('e8514c100090')),
    (0x107800, bytes.fromhex('00000000000000000000000000000000000000000000000000000000000000000000'), bytes.fromhex('4883ec484c895424304c895c243831c048894424204889442428488b03488b804802')),
    (0x107824, bytes.fromhex('0000000000'), bytes.fromhex('4889d9ba01')),
    (0x10782c, bytes.fromhex('000000'), bytes.fromhex('41b801')),
    (0x107832, bytes.fromhex('000000000000000000000000000000'), bytes.fromhex('4c8d4c2420ffd0488b03488b804802')),
    (0x107843, bytes.fromhex('0000000000'), bytes.fromhex('4889d9ba0a')),
    (0x10784b, bytes.fromhex('000000'), bytes.fromhex('41b801')),
    (0x107851, bytes.fromhex('0000000000000000000000000000000000'), bytes.fromhex('4c8d4c2428ffd0488b44242848898424c0')),
    (0x107865, bytes.fromhex('000000000000000000000000000000'), bytes.fromhex('488b4424204885c074304889d9ba0a')),
    (0x107877, bytes.fromhex('000000'), bytes.fromhex('41b801')),
    (0x10787d, bytes.fromhex('0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000'), bytes.fromhex('4c8d4c24204c8b1b4d8b5b4041ffd3488b4c24204885c97409488b01488b4010ffd04c8b5424304c8b5c2438488b034531c94883c448c3cccccccccccccccccccccccc4883ec484c895424304c895c2438488b8424c0')),
    (0x1078d6, bytes.fromhex('00000000000000000000'), bytes.fromhex('48894424204889d9ba0a')),
    (0x1078e3, bytes.fromhex('000000'), bytes.fromhex('41b801')),
    (0x1078e9, bytes.fromhex('0000000000000000000000000000000000000000000000000000000000000000000000000000000000'), bytes.fromhex('4c8d4c24204c8b1b4d8b5b4041ffd3488b4c24204885c97409488b01488b4010ffd031c048898424c0')),
    (0x107915, bytes.fromhex('000000000000000000000000000000000000000000'), bytes.fromhex('4c8b5424304c8b5c2438488b034531c94883c448c3')),
]

def sha(b): return hashlib.sha256(b).hexdigest()

def rebuild(source: bytes) -> bytes:
    if sha(source) != EXPECTED_SOURCE_SHA:
        raise ValueError(f'source SHA mismatch: {sha(source)} != {EXPECTED_SOURCE_SHA}')
    b=bytearray(source)
    for off,old,new in PATCHES:
        got=bytes(b[off:off+len(old)])
        if got != old:
            raise ValueError(f'guard mismatch at {off:#x}: {got.hex()} != {old.hex()}')
        b[off:off+len(new)] = new
    out=bytes(b)
    if sha(out) != EXPECTED_TARGET_SHA:
        raise ValueError(f'target SHA mismatch: {sha(out)} != {EXPECTED_TARGET_SHA}')
    return out

if __name__ == '__main__':
    import argparse
    ap=argparse.ArgumentParser()
    ap.add_argument('source')
    ap.add_argument('output')
    ns=ap.parse_args()
    out=rebuild(Path(ns.source).read_bytes())
    Path(ns.output).write_bytes(out)
    print(f'{LABEL}: EXACT PASS {sha(out)} patches={len(PATCHES)}')
