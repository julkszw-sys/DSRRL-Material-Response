#!/usr/bin/env python3
"""V249 operator-local patch contract; no Windows runtime/game dependency."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "renderer-core/tools"))
from apply_v249_primary_source_join import (
    patch_flver, patch_source, FLVER, SOURCE, FLVER_NEW, SOURCE_NEW
)

flver = FLVER.read_text(encoding="utf-8").replace("\r\n", "\n")
source = SOURCE.read_text(encoding="utf-8").replace("\r\n", "\n")
new_flver = patch_flver(flver)
new_source = patch_source(source)
assert new_flver.count("pmetal_env_source_selector_clear();") == 1
assert "  material_owner_selection_clear();" in new_flver
assert "pmetal_env_source_selector_event(" in new_flver
assert "pmetal_producer_state_clear();" in new_source
assert "pmetal_producer_state_begin(" in new_source
assert new_source.count("if (latest_hook_source(out)) {") == 1
assert "if (hook_ready) {" in new_source
assert "out = hook_source;" in new_source
assert "stage=%u main=%u hook=%u exact_payload=%u" in new_source
assert "main_ready && hook_ready" in new_source
assert "sidecar_20261010::exact_nonmetal_material(material) &&" in new_source
assert new_source.count("V249 PMETAL SOURCE JOIN") == 1
assert new_source.count("same_hook_source_payload(main_source, hook_source)") == 1
# Exact source identity and material authority retained, fail open on miss.
assert new_source.count("exact_pmetal_material_selection(material)") >= 2
assert "#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)" in new_source
assert "v220_nonmetal_join" in new_source
assert "DSRRL_V223_EXACT_NONMETAL_SHARED_LIGHTBANK_FALLBACK" in new_source
# Idempotent postcondition
assert patch_flver(new_flver) == new_flver
assert patch_source(new_source) == new_source
print("V249_SOURCE_HARD_GATES_AND_POSTCONDITION_PASS")
print("V249_PIXEL_BEHAVIOR_OPEN_PRIMARY_AND_HOOK_CANDIDATE")
