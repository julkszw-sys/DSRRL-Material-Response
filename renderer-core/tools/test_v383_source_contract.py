#!/usr/bin/env python3
"""V383 cross-material read-only build contract; does not claim runtime or pixels."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
flver=(root/'src/runtime/flver_engine_hooks.cpp').read_text()
source=(root/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
header=(root/'include/dsrrl/runtime/v383_remo_source_witness.hpp').read_text()
build=(root/'tools/build_v230_production.ps1').read_text()
assert flver.count('v383_model_material_selector(owner,descriptor,identity,')==1
assert flver.index('v383_model_material_selector(owner,descriptor,identity,')<flver.index('bool pointlight_spc = false;')
assert source.count('v383_native_lightbank_producer(source_a,source_b,selector_a,selector_b,')==1
assert source.count('[DSRRL V383 NATIVE NEGATIVE]')==2
assert '[DSRRL V383 REMO MODEL MATERIAL]' in header
assert '[DSRRL V383 NATIVE LIGHTBANK]' in header
assert 'TAE180_input=NOT_SAMPLED' in header
assert 'model_default_DE=NOT_SAMPLED' in header
assert 'v383_remo_source_witness.hpp' in source and 'v383_remo_source_witness.hpp' in flver
assert 'V383 NATIVE LIGHTBANK' in build and 'V383 REMO MODEL MATERIAL' in build
a=source.index('bool latest_hook_source(\n')
b=source.index('\nvoid clear_hook_source()',a)
protected=source[a:b]
assert 'v383_' not in protected
assert 'g_hook_source_tls.valid' in protected
assert 'g_hook_source_global.source' in protected
assert 'latest_hook_source(out)' in source
for forbidden in ['source.row_id_a =','source.row_id_b =','selector_a = 5','selector_b = 5','D3D11_MAP_WRITE']:
    assert forbidden not in header
print('V383 cross-material source witness source contract PASS; native and pixel separately gated')
