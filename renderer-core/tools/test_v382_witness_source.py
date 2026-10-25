#!/usr/bin/env python3
"""Static V382 diagnostic non-interference contracts."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
src=(ROOT/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
header=(ROOT/'include/dsrrl/runtime/v382_source_owner_witness.hpp').read_text()
a=src.index('bool latest_hook_source(\n')
b=src.index('\nvoid clear_hook_source()',a)
protected=src[a:b]
assert 'if (g_hook_source_tls.valid)' in protected
assert 'g_hook_source_global.source' in protected
assert 'v382_source_owner_witness' not in protected
assert 'latest_hook_source(out)' in src
assert 'out.consumer_source_kind = 1u;' in src
assert 'out.consumer_source_kind = 2u;' in src
assert src.count('v382_source_owner_witness(')==2
assert '[DSRRL V382 SOURCE OWNER READONLY]' in header
assert 'actor_join_proven=0' in header
for forbidden in ['envdiffuse_a[0] =','envdiffuse_a[1] =','envdiffuse_a[2] =', 'source.row_id_a =', 'source.row_id_b =']:
    assert forbidden not in header,forbidden
print('V382 source-owner read-only static contracts PASS')
