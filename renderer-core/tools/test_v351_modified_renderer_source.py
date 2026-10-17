#!/usr/bin/env python3
"""V351 source-only contract: provenance is distinct from operator enablement.
Static test does NOT assert PTDE pixel equivalence or 100% source coverage.
Run after materializers V304 then V351.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
d=(r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
s=(r/"src/runtime/pmetal_env_source_runtime.cpp").read_text("utf-8")
o=(r/"src/runtime/material_owner_selection.cpp").read_text("utf-8")
b=(r/"include/dsrrl/runtime/v300_dsr_only_lightbank_donors.hpp").read_text("utf-8")
recovery=(r/"include/dsrrl/runtime/v351_exact_material_route_recovery.hpp").read_text("utf-8")
assert 'v300::decode_linear(dsr.dsr_envdiffuse)' in s
assert 'v300::decode_linear(typed.dsr_values->dsr_envspec)' in s
assert 'source.bank_signature_a == dsr.signature' in d
assert 'source.bank_signature_b == dsr.signature' in d
assert '#if defined(DSRRL_V351_TYPED_DSR_ONLY_OPERATOR)' in d
assert 'v351_typed_source_invalid_RELEASE_BLOCK' in d
assert 'typed_source_into_modified_renderer' in d
assert 'material_sidecars=KEEP_DRAW_ATOMIC' in d
assert 'v304_typed_dsr_only_native_draw' in d
assert d.index('#if defined(DSRRL_V351_TYPED_DSR_ONLY_OPERATOR)') < d.index('"v304_typed_dsr_only_native_draw"')
assert 'v351::recover_exact_route(identity)' in o
assert 'g_current = resolved;' in o
assert 'mr::exact_runtime_material_response_identity(candidate)' in recovery
assert 'matches > 1u' in recovery
assert 'mr::exact_runtime_material_response_identity(identity)' in recovery
assert 'identity.owner_tuple_exact' in recovery and 'identity.actual_material_exact' in recovery
assert '0x1ecfd1e617c59071ULL' in b
assert '{{255u,255u,255u,150u}}' in b
assert 'latest_hook_source(out)' in s
print('V351_SOURCE_CONTRACT_PASS: exact typed DSR-only source goes to existing modified operator; route recovery exact-only')
