#!/usr/bin/env python3
"""V359 PTDE EnvDiffuse independent native t11/t13 router source contract.
C++ unit test verifies exact registry parsing and duplicate rejection.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
runtime=(r/"src/runtime/envspec_resource_runtime.cpp").read_text("utf-8")
api=(r/"include/dsrrl/runtime/envspec_resource_runtime.hpp").read_text("utf-8")
draw=(r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
header=(r/"include/dsrrl/operators/env_spec/native_envdiffuse_identity.hpp").read_text("utf-8")
tests=(r/"tests/test_native_envdiffuse_identity.cpp").read_text("utf-8")
assert 'is_native_envdiffuse_desc' in runtime
assert 'desc.texture.width != k_dsr_diffuse_size' in runtime
assert 'k_dsr_diffuse_tight_bc6h_bytes = 24576u' in runtime
assert 'load_native_diffuse_hash_atlas' in runtime
assert 'hashing::sha256(bytes.data(), bytes.size())' in runtime
assert 'unique_native_diffuse_probe(g_native_diffuse_hashes,digest)' in runtime
assert 'g_native_diffuse_resources' in runtime
assert 'g_diffuse_by_view' in runtime
assert 'prepare_envdiffuse_bound(' in runtime
assert 'prepare_envdiffuse_bound(' in api
assert 'pixel_srv_shadow_snapshot(' in draw
assert 'cmd_list, 11u, envdiffuse_view_count' in draw
assert 'context->PSGetShaderResources(' in draw
assert '11u, envdiffuse_view_count, stock_envdiffuse_views' in draw
assert 'env_resources_.prepare_envdiffuse_bound(' in draw
assert 'env_resources_.prepare_envdiffuse(' not in draw
assert 'prepared.env_resources.probe_a,' not in draw[draw.find('#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)',draw.find('effect_latch(effect_b12_ready_);')):draw.find('if (!g_native_envdiffuse_logged.exchange(')]
assert 'prepare_envdiffuse(a,b,probe_b_required,prepared)' in runtime
assert 'DSR_NATIVE_ENVDIFFUSE_PROBE_HASH_V1.bin' in header
assert 'cd3549ee' not in tests  # tests use synthesized exact-width fixtures
assert 'native_diffuse_hash_atlas' in tests
assert 'k_native_envdiffuse_count = 342u' in header
assert 'if (found.has_value())' in header, "colliding m17 SHA must fail-open"
print("V359 INDEPENDENT ENV DIFFUSE SOURCE CONTRACT PASS; build/runtime/pixel separately OPEN")
