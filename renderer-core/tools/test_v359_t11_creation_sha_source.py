#!/usr/bin/env python3
"""T11 creation diagnostic stays read-only, separately scoped from PTDE activation."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=(root/"src/runtime/envspec_resource_runtime.cpp").read_text(encoding="utf-8")
builder=(root/"tools/build_v230_envdiffuse_cinematic_trace.ps1").read_text(encoding="utf-8")
assert source.count("[DSRRL V359 T11 CREATE SHA]")==1
assert source.count("v359_log_t11_creation(")>=7
for stage in ("NO_INITIAL_DATA","EXTRACT_FAILED","HASH_NOT_UNIQUE_ATLAS",
              "MATCH_REGISTERED","ATLAS_NOT_READY","DEVICE_MISMATCH",
              "REGISTRY_DEVICE_LOST","ATLAS_LOST"):
    assert stage in source,stage
assert 'unique_native_diffuse_probe(g_native_diffuse_hashes,digest)' in source
assert 'g_native_diffuse_resources[resource_handle.handle] = {probe.value()}' in source
assert 'v359_log_t11_creation("MATCH_REGISTERED"' not in source
assert 'action=READ_ONLY' in source
assert "DSRRL_V359_T11_CREATION_SHA_WITNESS" in builder
assert 'load_native_diffuse_hash_atlas' in source
assert 'return false;' in source
print("V359_T11_CREATION_SHA_SOURCE_CONTRACT_PASS; RUNTIME_AND_PIXELS_OPEN")
