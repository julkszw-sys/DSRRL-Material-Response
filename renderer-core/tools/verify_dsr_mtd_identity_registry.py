#!/usr/bin/env python3
"""Verify checked-in exact DSR MTD identity registry invariants.

Construction-only verifier: it does not claim runtime activation or pixel equivalence.
The current registry is intentionally partial; absence must remain UNKNOWN/fail-open.
"""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
P=ROOT/'include/dsrrl/operators/material_response/generated_dsr_mtd_identity_v1.hpp'
text=P.read_text(encoding='utf-8')
assert 'k_dsr_mtd_identity_source_complete=false' in text, 'partial registry must not claim source completeness'
rows=re.findall(r'\{0x([0-9a-f]{16})ull,\{([^}]*)\},(true|false)\}',text)
assert rows, 'identity registry is empty'
hashes=[int(h,16) for h,_,_ in rows]
assert hashes==sorted(hashes), 'semantic hashes must be sorted for binary search'
assert len(hashes)==len(set(hashes)), 'duplicate semantic hash requires explicit ambiguity representation, not duplicate rows'
for h,raw,amb in rows:
    bs=re.findall(r'0x([0-9a-f]{2})',raw)
    assert len(bs)==32, f'{h}: raw SHA-256 must contain 32 bytes'
    if amb=='false': assert any(int(x,16) for x in bs), f'{h}: resolved identity may not have zero SHA'
# P_Metal exact identity is protected and must remain pinned to canonical raw DSR MTD SHA.
pm='fd72a0409ae13e45'; expected='ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b'
match=next((r for r in rows if r[0]==pm),None)
assert match is not None, 'P_Metal exact identity missing'
actual=''.join(re.findall(r'0x([0-9a-f]{2})',match[1]))
assert match[2]=='false' and actual==expected, 'P_Metal raw MTD identity drift'
print(f'DSR_MTD_IDENTITY_REGISTRY_PASS records={len(rows)} source_complete=false pmetal=exact unknown_policy=fail_open')
