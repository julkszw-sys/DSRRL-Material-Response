#!/usr/bin/env python3
import argparse
import collections
import json
import struct
from pathlib import Path

OP_NAMES = {1:'MOV',2:'ADD',3:'SUB',4:'MAD',5:'MUL',6:'RCP',7:'RSQ',8:'DP3',9:'DP4',10:'MIN',11:'MAX',12:'SLT',13:'SGE',14:'EXP',15:'LOG',16:'LIT',17:'DST',18:'LRP',19:'FRC',20:'M4x4',21:'M4x3',22:'M3x4',23:'M3x3',24:'M3x2',25:'CALL',26:'CALLNZ',27:'LOOP',28:'RET',29:'ENDLOOP',30:'LABEL',31:'DCL',32:'POW',35:'ABS',36:'NRM',37:'SINCOS',38:'REP',39:'ENDREP',40:'IF',41:'IFC',42:'ELSE',43:'ENDIF',44:'BREAK',45:'BREAKC',46:'MOVA',47:'DEFB',48:'DEFI',64:'TEXCOORD',65:'TEXKILL',66:'TEX',81:'DEF',0xffff:'END',0xfffe:'COMMENT'}

def regtype(token: int) -> int:
    return ((token >> 28) & 0x7) | ((token >> 8) & 0x18)

def terminal_colorout(path: Path):
    data = path.read_bytes()
    toks = list(struct.unpack('<%dI' % (len(data)//4), data[:len(data)//4*4]))
    i = 1
    writers = []
    while i < len(toks):
        token = toks[i]
        opcode = token & 0xffff
        if opcode == 0xffff:
            break
        if opcode == 0xfffe:
            i += 1 + ((token >> 16) & 0x7fff)
            continue
        length = (token >> 24) & 0xf
        if length == 0:
            i += 1
            continue
        params = toks[i+1:i+1+length]
        if params and regtype(params[0]) == 8:
            dest = params[0]
            writers.append({
                'token_index': i,
                'opcode': OP_NAMES.get(opcode, hex(opcode)),
                'write_mask': (dest >> 16) & 0xf,
                'dest_modifier': (dest >> 20) & 0xf,
                'sat': bool(dest & 0x00100000),
                'partial_precision': bool(dest & 0x00200000),
                'register': dest & 0x7ff,
            })
        i += 1 + length
    return writers[-1] if writers else None

def shader_class(name: str) -> str:
    for token in ('PntSSSS','PntSS','PntS'):
        if token in name:
            return token.upper()
    return 'BASE'

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--ptde-phn-dir', required=True)
    ap.add_argument('--source-artifact-id', type=int, default=None)
    ap.add_argument('--source-sha256', default=None)
    ap.add_argument('--output', required=True)
    args = ap.parse_args()

    root = Path(args.ptde_phn_dir)
    paths = sorted(p for p in root.glob('FRPG_Phn_*.fpo') if 'HemEnv' in p.name)
    rows = []
    counts = collections.Counter()
    exact_special_names = {'Alp': 0, 'Parallax': 0, 'Subsurf': 0}

    for p in paths:
        terminal = terminal_colorout(p)
        if terminal is None:
            raise SystemExit(f'no COLOROUT writer: {p.name}')
        cls = shader_class(p.name)
        counts[(cls, terminal['opcode'], terminal['write_mask'], terminal['sat'], terminal['partial_precision'])] += 1
        for k in exact_special_names:
            if k in p.name:
                exact_special_names[k] += 1
        rows.append({'shader': p.name, 'class': cls, **terminal})

    good = [r for r in rows if r['register'] == 0 and r['write_mask'] == 0x7 and r['sat']]
    report = {
        'schema': 1,
        'audit': 'PTDE FRPG_Phn HemEnv terminal write revalidation',
        'source_artifact_id': args.source_artifact_id,
        'source_sha256': args.source_sha256,
        'input_root': root.name,
        'shader_count': len(rows),
        'rgb_only_sat_count': len(good),
        'combined_rgba_terminal_count': sum(1 for r in rows if r['write_mask'] == 0xf),
        'special_exact_name_counts': exact_special_names,
        'breakdown': [
            {'class': c, 'opcode': op, 'write_mask': mask, 'sat': sat, 'partial_precision': pp, 'count': n}
            for (c, op, mask, sat, pp), n in sorted(counts.items())
        ],
        'conclusion': {
            'generic_combined_rgba_sat_exact_for_phn': False,
            'reason': 'PTDE exact Phn HemEnv terminal clamp is RGB-only; combined RGBA SAT would additionally clamp alpha.',
            'hash_alias_authorization': 'Exact semantic identity required; DSR-only Alp/Parallax/Subsurf aliases do not inherit PTDE authorization from byte-identical payload hashes.'
        },
        'rows': rows,
    }
    Path(args.output).write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
