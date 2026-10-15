#!/usr/bin/env python3
"""V290 exact 1295 first-consumer reachability and no auto-authorization guard."""
from pathlib import Path
import importlib.util
import runpy
import tempfile
root=Path(__file__).resolve().parents[2]
runpy.run_path(str(root/"renderer-core/tools/test_v289_nonmetal_lerp_r8_scope.py"),run_name="__main__")
def read(p):return (root/p).read_text(encoding="utf-8")
header=read("renderer-core/include/dsrrl/runtime/v290_spec_draw_census.hpp")
addon=read("renderer-core/integrated/integrated_addon.cpp")
cmake=read("renderer-core/integrated/CMakeLists.txt")
assert "first_seen" in header and "8192u" in header
assert "NOT_IN_OCT10_OR_V12" in header
assert "SIDECAR_NOT_READY" in header
assert "NO_EXACT_PTDE_CONSUMER" in header
assert "ATOMIC_CANDIDATE" in header
assert "NO_FLVER_OWNER" in header
assert 'DSRRL V290 SPEC DRAW CENSUS' in header
assert addon.count("v290_spec_census::claim(")==2
assert addon.count("probe_exact_specular_companion(")>=2
assert addon.count("v290_spec_census::report(")==2
assert "if (!g_pmetal_envspec.prepare(" in addon
assert "DSRRL_V290_FULL_SIDECAR_DRAW_CENSUS" in cmake
assert "NOT DSRRL_V285_ATOMIC_NONMETAL_DIAG" in cmake
assert "g_material_resources.drop_spec_rgb_request(" in addon
import sys
sys.path.insert(0,str(root/"renderer-core/tools"))
from generate_sidecar_spec_whitelist_20261010 import load_names,fnv1a_ascii_lower
from v290_sidecar_draw_report import parse
sidecar=root/"renderer-core/data/provenance/sidecar_specular_20261010_names.b85"
names=load_names(sidecar)
assert len(names)==1295
with tempfile.TemporaryDirectory() as td:
    path=Path(td)/"fixture.log"
    spec=f"{fnv1a_ascii_lower(names[0]):016x}"
    key="rx=33 family=1 route=1073741869 slot=2 mtd=0f2b9a1012b83c4d flver=dd9fdcf0770da2db"
    path.write_text(
        f"[DSRRL V290 SPEC DRAW CENSUS] cut=ATOMIC_CANDIDATE {key} material=1 owner=1 stock_t1=1 logical=1 manifest=1 file=1 spec={spec}\n"+
        f"[DSRRL V287 MAT TRACE] stage=atomic_indexed_draw_issued {key} exact=1 valid=1\n",
        encoding="utf-8")
    summary,rows=parse(path,sidecar)
    assert summary["sidecar_spec_names_total"]==1295
    assert summary["unique_manifest_names_seen_bound_t1"]==1
    assert summary["unique_manifest_names_with_draw_witness_in_sample"]==1
    assert rows[0]["sidecar_name"]==names[0]
    assert rows[0]["draw_witness_for_tuple"]==1
print("V290_MANIFEST_TO_DRAW_COVERAGE_CONTRACT_PASS full_names=1295 native_t1=observed only gate=unchanged")
print("IN_GAME_1295_COVERAGE_AND_PIXEL_OPEN")
