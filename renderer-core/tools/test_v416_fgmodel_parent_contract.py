#!/usr/bin/env python3
"""V416 fail-open reciprocal FgModel/child source contract, no pixel claims."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sel=(root/'src/runtime/flver_engine_hooks.cpp').read_text(encoding='utf-8')
producer=(root/'src/runtime/pmetal_env_source_runtime.cpp').read_text(encoding='utf-8')
stamp=(root/'include/dsrrl/runtime/v415_model_draw_identity.hpp').read_text(encoding='utf-8')
build=(root/'tools/build_v416_fgmodel_parent_diagnostic.ps1').read_text(encoding='utf-8')
workflow=(root.parent/'.github/workflows/v416-fgmodel-parent-identity.yml').read_text(encoding='utf-8')

assert 'DSRRL_V415_FLVER_MODEL_ID_DIAG' in build
assert 'DSRRL_V416_FGMODEL_PARENT_DIAG' in build
assert 'build-v416-fgmodel-parent-diagnostic' in build
assert 'DSRRL_V416_FgModel_Parent_Diagnostic.addon64' in build
assert 'build_v416_fgmodel_parent_diagnostic.ps1' in workflow
assert 'test_v416_fgmodel_parent_contract.py' in workflow
assert 'DSRRL_V416_FgModel_Parent_Diagnostic.addon64' in workflow

# Binary RE preimage for original 2022 DSR:
# FgFlverModel ctor RVA20C6E0 writes ctor R9 into child+0x140.
# FgModel ctor RVA76E260 writes exact RTTI vtable13DF6D0,
# FgModel ctor later saves FgFlverModel* in parent+0x28.
assert 'class_kind==1u' in sel
assert 'model_addr+0x140u' in sel
assert 'fg_parent_addr+0x28u' in sel
assert 'fg_parent_addr+0x30u' in sel
assert 'fg_parent_vtable!=g_base+0x13DF6D0u' in sel
assert 'backlink!=model_addr' in sel
assert 'range_ok(reinterpret_cast<const void *>(' in sel
assert 'v415::record_verified_fgparent(' in sel
assert 'fg_reason=5u' in sel
assert '[DSRRL V416 FGMODEL PARENT]' in sel
assert '[DSRRL V416 FGMODEL SOURCE JOIN]' in producer
assert 'model_v415.fg_parent_reciprocal' in producer
assert 'native_producer_seq=%llu' in producer

# The source can still carry all R43 and exact DSR lightbank values.
assert 'latest_hook_source_exact_selector(' in producer
assert 'if (latest_hook_source(out))' in producer
assert 'pmetal_producer_state_publish(' in producer
assert 'read-only' in sel or 'NEVER guess actors' in sel
assert 'actor_lifetime=OPEN' in producer
assert 'pixel=OPEN' in producer
assert 'fg_parent_reciprocal = true;' in stamp
assert 'g_selector_model.class_kind != 1u' in stamp
print('V416 FGMODEL bidirectional identity observer SOURCE CONTRACT PASS; native actor/pixel OPEN')
