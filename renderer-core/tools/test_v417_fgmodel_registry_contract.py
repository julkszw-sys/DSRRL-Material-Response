#!/usr/bin/env python3
"""V417 read-only original executable owner-provenance source contract.

Static ABI attested for pinned DSR executable. In-game actor and pixel OPEN.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
sel=(r/'src/runtime/flver_engine_hooks.cpp').read_text()
producer=(r/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
hdr=(r/'include/dsrrl/runtime/v415_model_draw_identity.hpp').read_text()
build=(r/'tools/build_v417_fgmodel_registry_diagnostic.ps1').read_text()
workflow=(r.parent/'.github/workflows/v417-fgmodel-registry-identity.yml').read_text()
assert 'DSRRL_V415_FLVER_MODEL_ID_DIAG' in build
assert 'DSRRL_V416_FGMODEL_PARENT_DIAG' in build
assert 'DSRRL_V417_FGMODEL_REGISTRY_DIAG' in build
assert 'DSRRL_V417_FgModel_Registry_Diagnostic.addon64' in build
assert 'build_v417_fgmodel_registry_diagnostic.ps1' in workflow
assert 'test_v417_fgmodel_registry_contract.py' in workflow
assert 'DSRRL_V417_FgModel_Registry_Diagnostic.addon64' in workflow
assert 'fg_reason==5u' in sel
assert 'fg_parent_addr+0x38u' in sel
assert 'fg_parent_addr+0x40u' in sel
assert 'fg_upstream!=0u' in sel
assert 'range_ok(reinterpret_cast<const void *>(fg_upstream)' in sel
assert 'v415::record_registry_observation(' in sel
assert '[DSRRL V417 FG REGISTRY PROVENANCE]' in sel
assert 'registry_lifetime=OPEN upstream_class=OPEN' in sel
assert '[DSRRL V417 FG REGISTRY SOURCE JOIN]' in producer
assert 'model_v415.fg_parent_reciprocal' in producer
assert 'registry_lifetime=OPEN actor=OPEN gpu=OPEN pixel=OPEN' in producer
assert 'fg_registry_id=0u' in hdr and 'fg_refcount=0u' in hdr
assert 'g_selector_model.fg_parent_reciprocal' in hdr
assert 'g_selector_model.fg_registry_id=registry_id' in hdr
# No new native hook or LightBank target modifications.
assert 'pmetal_producer_state_publish(' in producer
assert 'latest_hook_source_exact_selector(' in producer
assert 'if (latest_hook_source(out))' in producer
assert 'record_registry_observation(' not in producer
print('V417 bounded native FgModel registration provenance SOURCE CONTRACT PASS')
