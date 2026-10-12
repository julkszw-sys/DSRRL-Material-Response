#!/usr/bin/env python3
"""V241 pure source provenance, precision and idempotence tests."""
from pathlib import Path
import importlib.util
p=Path(__file__).with_name('apply_v241_m12_probe_prewarm.py')
spec=importlib.util.spec_from_file_location('warm',p)
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
root=Path(__file__).resolve().parents[2]
s=(root/m.PATH).read_text(encoding='utf-8')
assert m.patch(s)==s
n=m.patch(s,True)
assert n!=s
assert m.patch(n,True)==n
assert m.patch(n)==n
assert n==s.replace(m.ANCHOR_INCLUDE,m.NEW_INCLUDE,1).replace(m.ANCHOR_DECL,m.INSERT_DECL,1).replace(m.ANCHOR_BODY,m.INSERT_BODY,1)
for corrupt in (s.replace(m.ANCHOR_DECL,"void corrupt_device()"),s.replace(m.ANCHOR_BODY,"BAD"),s.replace('g_ptde_cubes.emplace(','BAD(')):
    try:m.patch(corrupt,True)
    except ValueError:pass
    else:raise AssertionError('unexpected allowed source drift')
assert 'k_m12_pair[] = {130u, 40u}' in m.INSERT_BODY
assert 'for (std::uint8_t slot = 0u; slot < 3u; ++slot)' in m.INSERT_BODY
assert m.INSERT_BODY.count('get_ptde_cube(')==1
assert m.INSERT_BODY.count('get_ptde_envdiffuse_cube(')==1
assert m.INSERT_BODY.index('if (pack_ready && sampler_ready)')<m.INSERT_BODY.index('get_ptde_cube(')
assert 'create_resource(' not in m.INSERT_BODY and 'get_ptde_cube(' in m.INSERT_BODY
assert 'std::lock_guard' not in m.INSERT_BODY
assert 'runtime_hitch=OPEN' in m.INSERT_BODY
print('V241_M12_PAIR_SOURCE_SCOPE_AND_FAIL_OPEN_PASS')
