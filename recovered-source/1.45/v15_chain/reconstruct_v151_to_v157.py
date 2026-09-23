#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path

EXPECTED = {
    'v151': '7f10f908d598f8d6b952e2c78bce6eb97a8b84a297f956a069c439b7977b65cc',
    'v153': '4641d2f9d66e879cc86c05aa403a6f9fcfe96c5f9486c15cdae4339f15d50d68',
    'v154': '4ddf2b270b09efcc57c4cf680ac0da78ebc3f0a5f58fd7d242b87b90aa6168cc',
    'v155': 'bf552c8606e27ab1bf3535b27509e91d8692fe553993a0e80b767cd30c1dd283',
    'v156': '9f112214a7cfc058775df26c58d4e8afaaa40fa22015613520bcaafcc9d5d21b',
    'v157': 'cfd1fe585710497a411adad8dc7edd9b46ae187133ee0625abeed2ab1ab96f09',
}

# Exact byte deltas recovered by pairwise comparison of owner archival runtime packages.
# Offsets are file offsets in the 35,423,232-byte PE.
PATCHES = {
    'v153': [
        (0x195470, '4cb40100', '9cf3ffff'),
        (0x195485, '87cd0100', '07ffffff'),
        (0x1954f8, 'f4b20100', '74eaffff'),
        (0x1b7834, '6163746976652065786163742d736c', '5631352e332073746174652d6c6179'),
        (0x1b7844, '742062696e6420646961676e6f73746963', '757420706978656c2d696e657274000000'),
    ],
    'v154': [
        (0x1b05a8, '488b9688221100', 'e9150100009090'),
    ],
    'v155': [
        (0x1b1cf6, '0023', '6022'), (0x1b1d90, '0423', '6422'), (0x1b1dac, '0423', '6422'),
        (0x1b1e42, '0023', '6022'), (0x1b23c3, '0823', '6822'), (0x1b23d4, '0923', '6922'),
        (0x1b23e6, '0a23', '6a22'), (0x1b23f8, '0b23', '6b22'), (0x1b240a, '0c23', '6c22'),
        (0x1b241c, '0d23', '6d22'), (0x1b242e, '0e23', '6e22'), (0x1b2440, '0f23', '6f22'),
        (0x1b2452, '1023', '7022'), (0x1b2464, '1123', '7122'), (0x1b2476, '1223', '7222'),
        (0x1b2488, '1323', '7322'), (0x1b249a, '1423', '7422'), (0x1b24ac, '1523', '7522'),
        (0x1b24be, '1623', '7622'),
    ],
    'v156': [
        (0x1b05a8, 'e9150100009090', '488b9688221100'),
        (0x1b42a2, '0823', '6822'), (0x1b42ad, '0923', '6922'), (0x1b42b9, '0a23', '6a22'),
        (0x1b42c5, '0b23', '6b22'), (0x1b42d1, '0c23', '6c22'), (0x1b42dd, '0d23', '6d22'),
        (0x1b42e9, '0e23', '6e22'), (0x1b42f5, '0f23', '6f22'), (0x1b4301, '1023', '7022'),
        (0x1b430d, '1123', '7122'), (0x1b4319, '1223', '7222'), (0x1b4325, '1323', '7322'),
        (0x1b4331, '1423', '7422'), (0x1b433d, '1523', '7522'), (0x1b4349, '1623', '7622'),
    ],
    'v157': [
        (0x195470, '9cf3ffff', '4cb40100'),
        (0x195485, '07ffffff', '87cd0100'),
        (0x1954f8, '74eaffff', 'f4b20100'),
        (0x1b7838, '33', '37'),
        (0x1b783a, '73746174652d6c61796f757420706978656c2d696e657274', '616374697665204253532d73616665000000000000000000'),
    ],
}

SEMANTICS = {
    'v153': 'restore PRE/PREPARE/POST to V12 targets; preserve V15.1 state/init/formatter layout (pixel inert)',
    'v154': 'bypass V15.1 per-thread A/B semantic-store block (pixel inert crash isolator)',
    'v155': 'relocate V15 static/BSS references from colliding 0x112300..0x112316 to safe 0x112260..0x112276',
    'v156': 're-enable V15.1 thread-state store and relocate previously missed unload/uninit BSS references',
    'v157': 'reactivate exact-slot EnvSpec PRE/PREPARE/POST wrappers on BSS-safe V15.6 basis',
}

def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def apply(data: bytes, stage: str) -> bytes:
    buf = bytearray(data)
    for off, old_hex, new_hex in PATCHES[stage]:
        old = bytes.fromhex(old_hex); new = bytes.fromhex(new_hex)
        assert len(old) == len(new)
        actual = bytes(buf[off:off+len(old)])
        if actual != old:
            raise SystemExit(f'{stage}: guard mismatch at 0x{off:x}: expected {old.hex()}, got {actual.hex()}')
        buf[off:off+len(new)] = new
    out = bytes(buf)
    got = sha256(out)
    if got != EXPECTED[stage]:
        raise SystemExit(f'{stage}: SHA mismatch expected={EXPECTED[stage]} got={got}')
    return out

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('v151')
    ap.add_argument('--out-dir', default='reconstructed')
    ap.add_argument('--audit', default=None)
    ns = ap.parse_args()
    base = Path(ns.v151).read_bytes()
    if sha256(base) != EXPECTED['v151']:
        raise SystemExit(f'v151 base SHA mismatch: {sha256(base)}')
    outdir = Path(ns.out_dir); outdir.mkdir(parents=True, exist_ok=True)
    current = base
    rows=[]
    for stage in ['v153','v154','v155','v156','v157']:
        current = apply(current, stage)
        p=outdir/f'{stage}.addon64'; p.write_bytes(current)
        rows.append({'stage':stage,'sha256':sha256(current),'size':len(current),'patch_count':len(PATCHES[stage]),'semantics':SEMANTICS[stage]})
        print(stage, sha256(current), len(current), 'PASS')
    audit={'schema':'dsrrl.material_response_1_45.envspec_v151_v157_exact_reconstruction.v1','base_v151_sha256':EXPECTED['v151'],'stages':rows,'final_v157_sha256':EXPECTED['v157'],'status':'EXACT_BINARY_RECONSTRUCTION_PASS','source_complete':False,'note':'This closes exact historical binary-delta provenance V15.1->V15.7. It does not replace the still-missing original V15.1 C source or pre-V12 source chain.'}
    apath=Path(ns.audit) if ns.audit else outdir/'RECONSTRUCTION_AUDIT.json'
    apath.write_text(json.dumps(audit,indent=2)+'\n',encoding='utf-8')
    return 0
if __name__=='__main__': raise SystemExit(main())
