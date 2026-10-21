#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
a=(root/'tools/materialize_v230_envdiffuse_cinematic_trace.py').read_text()
b=(root/'src/runtime/envspec_resource_runtime.cpp').read_text()
assert 'V359 T11 REJECT JOIN' in a
assert 'native_envdiffuse_probe_for_view' in a
assert 'native_envdiffuse_probe_for_view' in b
assert 'action=STOCK_DSR' in a
print('V359_T11_READONLY_REJECT_JOIN_SOURCE_PASS')
