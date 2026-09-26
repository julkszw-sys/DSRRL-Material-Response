#!/usr/bin/env python3
"""Verify the exact bridge-resource MTD semantic binding extension.
Construction/static-routing evidence only; UNKNOWN remains fail-open.
"""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p = ROOT / 'data/census/ptde_mtd_semantic_exact_bindings_v1.json'
d = json.loads(p.read_text(encoding='utf-8'))

assert d['claim_scope'] == 'CONSTRUCTION_AND_STATIC_ROUTING_ONLY'
assert d['runtime_activation'] == 'OPEN'
assert d['pixel_equivalence'] == 'OPEN'
assert 'exact MTD name + raw SHA' in d['identity_policy']
assert 'UNKNOWN is fail-open' in d['identity_policy']
rows = d['records']
assert len(rows) == 8
assert [r['bridge_binding_id'] for r in rows] == list(range(40, 48))

seen = set()
for r in rows:
    assert r['mtd_name'].endswith('.mtd')
    sha = r['raw_sha256']
    assert re.fullmatch(r'[0-9a-f]{64}', sha)
    assert sha not in seen
    seen.add(sha)
    assert r['material_family'] == 'DifSpcBmp'
    assert r['receiver_scope'] == [33, 34, 35]
    assert r['gate_policy'] in {'DIRECT_EXACT', 'PTDE_COMPANION_REQUIRED'}
    ops = r['operators']
    # Positive/negative claims here are route evidence, not name heuristics.
    assert ops['material_response'] == 'USE'
    assert ops['hemenv'] == 'USE'
    assert ops['pointlight'] == 'NO_USE'
    # Material-route identity alone cannot promote these consumers/operators.
    for op in ('spec_rgb', 'env_spec', 'subsurface'):
        assert ops[op] == 'UNKNOWN'

sem = d['semantics']
assert 'receiver family 33/34/35' in sem['hemenv_USE']
assert 'stable HemEnv route' in sem['pointlight_NO_USE']
assert 'does not prove PTDE donor/resource liveness' in sem['spec_rgb_UNKNOWN']
assert sem['unknown_policy'] == 'preserve host/fail-open'
print('MTD_SEMANTIC_EXACT_BINDINGS_PASS rows=8 ids=40..47 exact_sha=true runtime=OPEN pixel=OPEN')
