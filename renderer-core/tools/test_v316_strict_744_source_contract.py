#!/usr/bin/env python3
"""V316 static source/operator contract. Not a runtime or pixel test."""
from pathlib import Path
import importlib.util
root=Path(__file__).resolve().parents[1]
p=root/'tools/generate_sidecar_spec_whitelist_20261010.py'
sp=importlib.util.spec_from_file_location('v316_source',p)
m=importlib.util.module_from_spec(sp)
sp.loader.exec_module(m)
names=m.load_names(root/'data/provenance/sidecar_specular_20261010_names.b85')
low={'am_f_9450_l_s','am_m_9372_l_s','bd_m_9440_1_l_s','bd_m_9440_2_l_s','wp_a_0652_l_s'}
excluded_low={n for n in names if n.endswith('_l_s') and n not in low}
unknown={'wp_a_0911_s','wp_a_1004_s'}
direct=set(names)-excluded_low-unknown
strict=direct-low
assert len(names)==1295 and len(excluded_low)==544
assert len(direct)==749 and low<=direct
assert len(strict)==744 and not any(n.endswith('_l_s') for n in strict)
assert not(strict & unknown)
gate=(root/'include/dsrrl/runtime/v316_strict_744_sidecar_gate.hpp').read_text()
summary=(root/'include/dsrrl/runtime/v316_strict_744_runtime_summary.hpp').read_text()
src=(root/'src/runtime/material_resource_draw_runtime.cpp').read_text()
addon=(root/'integrated/integrated_addon.cpp').read_text()
fallback=(root/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
assert 'static_assert(strict_source_count()==744u' in gate
for n in low: assert f'"{n}"' in gate
cut='if(!v316::allow_sidecar_equipment_name(hash))return false;'
assert src.index(cut)<src.index('generated::spec_equipment_name_hash_allowed_v12(hash)')
assert src.count(cut)==1
assert addon.count('dsrrl::runtime::v315::summary();')==1
assert addon.count('dsrrl::runtime::v316::strict_summary();')==1
assert 'count_strict(v315::draw_issued|v315::state_restored)' in summary
assert 'if (latest_hook_source(out))' in fallback
assert 'v313::promote(' in addon and 'v314::report_stock_unrouted(' in addon
assert 'v316::promote(' not in addon and 'v316::draw(' not in addon
print('V316_STRICT_744_SOURCE_AND_READONLY_LEDGER_PASS')
print('RUNTIME_LIVENESS_OPEN_PIXEL_BEHAVIOR_OPEN')
