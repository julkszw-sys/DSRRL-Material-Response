#!/usr/bin/env python3
"""39 original PTDE legacy EnvDiffuse DDS and independent 342 PackedGI ownership."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
h=(root/'include/dsrrl/operators/env_spec/ptde_classic_envdiffuse_dds_sidecar_v1.hpp').read_text('utf8')
v=(root/'include/dsrrl/operators/env_spec/ptde_classic_envdiffuse_sidecar_reader_v361.hpp').read_text('utf8')
s=(root/'src/runtime/envspec_resource_runtime.cpp').read_text('utf8')
t=(root/'tools/materialize_v230_envdiffuse_cinematic_trace.py').read_text('utf8')
build=(root/'tools/build_v230_envdiffuse_cinematic_trace.ps1').read_text('utf8')
assert 'k_records=39u' in h and 'k_file_bytes=820208u' in h
for x in ('k_exact_sha256','legacy_plan::hashing::sha256(p,size)','legacy_plan::hashing::sha256(dds,','r.dds_offset!=next','next==size','std::memcmp(dds+84u,"DXT1"','r.format==1u','std::strcmp(r.name,expected)','std::strcmp(r.name,"EnvDif_def_000")'):
    assert x in v,x
for x in ('load_ptde_classic_pack','classic::validate','g_ptde_classic_pack.clear()','g_ptde_classic_ready=false','g_ptde_classic_cubes.reserve','get_ptde_classic_candidate','format::bc1_unorm','format::r8g8b8a8_unorm','resource_view_type::texture_cube','g_ptde_classic_cubes.emplace','device_ptr->destroy_resource_view'):
    assert x in s,x
assert 'DSRRL_V361_CLASSIC_ENVDIFFUSE_READER' in build
assert '[DSRRL V361 CLASSIC PTDE CANDIDATE]' in t
assert 'candidate?"GPU_READY":"NOT_READY"' in t
assert 'bind=NO t12=REJECTED pixels=OPEN' in t
assert 'native_legacy_envdiffuse_for_view(' in t
assert 'ptde_name' in t and 'EnvDif_m%u_%03u' in t
# Compiled feature cannot reassign canonical 342 EnvSpec/EnvDiffuse probe indices.
assert 'g_native_diffuse_resources[resource_handle.handle]=legacy_id' not in s
assert 'prepare_envdiffuse(legacy_identity.map' not in t
assert 'PSSetShaderResources(' not in t
print('V361_CLASSIC_39_DDS_READER_AND_NO_BIND_SOURCE_PASS')
