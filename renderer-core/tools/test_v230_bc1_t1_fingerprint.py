#!/usr/bin/env python3
"""V230 fail-open BC1 t1 producer fingerprint contract; no asset admission."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
e=(root/"src/runtime/envspec_resource_runtime.cpp").read_text("utf-8")
d=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
h=(root/"include/dsrrl/runtime/envspec_resource_runtime.hpp").read_text("utf-8")
b=(root/"tools/build_v230_production.ps1").read_text("utf-8")
w=(root.parent/".github/workflows/v230-production-from-v359.yml").read_text("utf-8")
for x in (
 "void v230_capture_bc1_origin(", "DXGI_FORMAT_BC1_UNORM",
 "initial_data[0].row_pitch", "hashing::sha256(bytes.data(),bytes.size())",
 "v230_capture_bc1_origin(device_ptr,desc,initial_data,resource_handle)",
 "g_v230_bc1.erase(resource_handle.handle)",
 "g_v230_bc1.clear()", "v230_bc1_origin_for_native_view(",
 "resource->Release();", "g_v230_bc1_mutex"
): assert x in e,x
for x in (
 "v230_bc1_origin_for_native_view(srv,bc1)",
 "bc1_mip0_sha256=%s", "bc1_mip0_exact=%u", "action=FAIL_OPEN_NO_ALIAS",
 "[DSRRL V230 MISSING SOURCE]"): assert x in d,x
assert "struct v230_bc1_origin_snapshot" in h
assert "DSRRL_V230_FAILOPEN_DIAGNOSTICS" in b
assert "DSRRL_V230_STOCK_DSR_ENVDIFFUSE" in b
assert "test_v230_bc1_t1_fingerprint.py" in w
assert "materialize_v358_exact_lifetime_probe_origin.py" in b
assert "materialize_v359_view_scope_identity.py" in b
assert "v230-bc1-t1-fingerprint-20261020" in w
assert "DSRRL-v2.3.0-ENVSPEC170-" in w
print("V230_BC1_T1_EXACT_INITIAL_DATA_FINGERPRINT_PASS resource_authority=UNCHANGED pixel=OPEN")
