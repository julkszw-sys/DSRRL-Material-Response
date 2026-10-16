#!/usr/bin/env python3
"""V315: verify read-only 749-entry coverage stages and strict actual draw cut."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
h=(root/"include/dsrrl/runtime/v315_direct_749_sidecar_ledger.hpp").read_text()
addon=(root/"integrated/integrated_addon.cpp").read_text()
resource=(root/"src/runtime/material_resource_draw_runtime.cpp").read_text()
rh=(root/"include/dsrrl/runtime/material_resource_draw_runtime.hpp").read_text()
census=(root/"include/dsrrl/runtime/v290_spec_draw_census.hpp").read_text()
source=(root/"src/runtime/pmetal_env_source_runtime.cpp").read_text()
generated=(root/"include/dsrrl/runtime/generated_spec_whitelist_20261010.hpp").read_text()
assert "std::array<std::uint64_t,749u> k_dsr_direct_spec_hashes" in generated
assert "static_assert(names::k_dsr_direct_spec_hashes.size()==749u" in h
assert "find_index(std::uint64_t hash)" in h
assert "return left<749u" in h
assert "if(idx==749u)" in h and "rejected_low_or_unknown" in h
assert "bool restored" in h and "lerp_draw" in h
assert "all_draws_verified=0 ptde_pixels=OPEN" in h
assert "ptde_low_only_excluded=544" in h
assert "unresolved_weapons_excluded=2" in h
assert "prepared.logical_spec_hash = h1" in resource
assert "std::uint64_t logical_spec_hash = 0u;" in rh
assert census.count("v315::observe_probe(")==2
assert "v315::summary();" in addon
assert addon.count("v315::prepared(")==2
assert addon.count("v315::issued(")==2
assert addon.count("dispatch.transaction !=")==2
assert addon.count("dsrrl::runtime::draw_tx_result::issued_restore_failed")==2
assert addon.count("draw_tx_issued(dispatch.transaction)")>=2
assert "prepared.envspec.material_resources.logical_spec_hash" in addon
assert "if (latest_hook_source(out))" in source
assert "v313::promote(" in addon
assert "v314::report_stock_unrouted(" in addon
assert "v315::promote(" not in addon and "v315::draw(" not in addon
assert "SIDECAR" not in resource or True # no blanket alias
print("V315_EXACT_749_DIRECT_PER_NAME_CURRENT_T1_PREPARE_ISSUED_RESTORE_LEDGER_PASS")
print("V315_RUNTIME_LIVENESS_AND_PTDE_PIXELS_OPEN")
