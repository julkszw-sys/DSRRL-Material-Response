#!/usr/bin/env python3
"""V359 reject draw t11 cause isolation; TLS producer hint is NOT resource identity."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=(root/'src/runtime/envspec_resource_runtime.cpp').read_text('utf8')
attestation=(root/'tools/materialize_v359_attested_gi_name_alias.py').read_text('utf8')
draw=(root/'tools/materialize_v230_envdiffuse_cinematic_trace.py').read_text('utf8')
assert source.count('tls_scope=THREAD_HINT_NOT_NATIVE_RESOURCE_PROOF')==1
assert source.count('tls_hooks=%u tls_present=%u tls_name=%.96s')==1
assert 'texture_identity_transport::status()' in source
assert 'texture_identity_transport::snapshot_raw(' in source
assert 'hook.name_hook_armed && hook.clear_hook_armed' in source
assert source.count('v359_log_t11_creation(')>=6
assert 'if (diffuse)' in source
assert 'g_native_diffuse_resources[resource_handle.handle] = {probe.value()}' in source
assert '#include "dsrrl/runtime/texture_identity_transport.hpp"' in attestation
assert 'name_status=%s' in draw and 'mapping=UNAUTHORIZED pixels=OPEN' in draw
a=source.index('const auto hook=texture_identity_transport::status();')
b=source.index('reshade::log::message(',a)
region=source[a:b]
for unwanted in ('PSSetShaderResources(', 'CreateTexture2D(', 'UpdateSubresource(', 'Map(', 'Unmap(', 'g_native_diffuse_resources[','v214_resource_provenance::instance().observe('):
    assert unwanted not in region,unwanted
print('V359_T11_CREATE_TLS_SOURCE_CONTRACT_PASS scope=THREAD_HINT_UNJOINED runtime_pixel=OPEN')
