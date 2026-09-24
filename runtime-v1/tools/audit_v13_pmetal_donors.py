#!/usr/bin/env python3
"""Audit the recovered Material Response 1.45 V13 P_Metal donor corpus.

This is a construction/provenance gate. It does not promote runtime or pixel
correctness. It proves that the committed Core 151 table is an exact
materialization of the preserved V13 donor source and that every bank range is
well-formed.
"""
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/'renderer-core/provenance/mr145/SOURCE_V13_PTDE_DONOR.cpp'
HDR=ROOT/'runtime-v1/generated/v13_pmetal_donors.hpp'

source=SRC.read_text(encoding='utf-8')
header=HDR.read_text(encoding='utf-8')

rows=[tuple(map(int,m.groups())) for m in re.finditer(
    r'\{(\d+)u,(\d+)u,(\d+)u,(\d+)u,(\d+)u\}',source)]
banks=[(m.group(1).lower(),int(m.group(2)),int(m.group(3))) for m in re.finditer(
    r'\{(0x[0-9a-fA-F]+)ULL,(\d+)u,(\d+)u\}',source)]

hrows=[tuple(map(int,m.groups())) for m in re.finditer(
    r'donor_row\{(\d+)u,(\d+)u,(\d+)u,(\d+)u,(\d+)u\}',header)]
hbanks=[(m.group(1).lower(),int(m.group(2)),int(m.group(3))) for m in re.finditer(
    r'bank_donor\{(0x[0-9a-fA-F]+)ULL,(\d+)u,(\d+)u\}',header)]

errors=[]
if len(rows)!=1246:
    errors.append(f'expected 1246 source donor rows, got {len(rows)}')
if len(banks)!=20:
    errors.append(f'expected 20 source banks, got {len(banks)}')
if rows!=hrows:
    errors.append('generated donor rows differ from preserved V13 source')
if banks!=hbanks:
    errors.append('generated bank table differs from preserved V13 source')
if len({sig for sig,_,_ in banks})!=len(banks):
    errors.append('duplicate V13 bank signature')
for sig,first,count in banks:
    if count<=0 or first<0 or first+count>len(rows):
        errors.append(f'bank {sig} has invalid row range first={first} count={count}')
if 'b12[2]=A, b12[3]=B' not in source:
    errors.append('V13 source no longer contains the exact A/B carrier declaration')
if 'no DSR g_draw or LightProbeParam.x' not in source:
    errors.append('V13 source no longer contains the source-gain exclusion declaration')

if errors:
    for e in errors:
        print('FAIL:',e,file=sys.stderr)
    raise SystemExit(1)

print(
    'V13 P_Metal donor audit: PASS '
    f'rows={len(rows)} banks={len(banks)} '
    'source->generated exact; bank ranges valid; semantic markers present'
)
