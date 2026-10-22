#!/usr/bin/env python3
"""V359 t11 witness only: source-anchored, read-only and separate from t12 auth."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'tools/materialize_v230_envdiffuse_cinematic_trace.py').read_text('utf-8')
assert s.count('[DSRRL V359 T11 REJECT JOIN]')==1
assert s.count('[DSRRL V359 T11 BOUND DESC]')==1
assert s.count('create_desc_candidate=%u')==1
assert s.count('tex.MipLevels==1u && tex.ArraySize==6u')==1
assert s.count('res->Release();')>=1
assert s.count('texture->Release();')>=1
assert 'if (!env_resource_ready) {' in s
assert 'action=STOCK_DSR pixels=OPEN' in s
a=s.index('// Read-only SRV dimension and native resource type witness.')
b=s.index('for(auto *v:diffuse)',a)
region=s[a:b]
for forbidden in ('PSSetShaderResources(', 'PSSetSamplers(', 'DrawIndexed(', 'CopyResource(', 'UpdateSubresource(', 'Map(', 'Unmap('):
    assert forbidden not in region,forbidden
print('V359_T11_BOUND_DESCRIPTOR_SOURCE_CONTRACT_PASS; runtime/pixel OPEN')
