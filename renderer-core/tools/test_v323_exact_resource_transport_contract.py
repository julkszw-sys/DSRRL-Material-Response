#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[2]
src=(root/"renderer-core/src/runtime/material_resource_draw_runtime.cpp").read_text()
require=["v214_resource_name_provenance.hpp","void on_init_resource(","void on_destroy_resource(","source_resource.handle","v214_resource_provenance::instance().snapshot(","native_backing","on_init_resource);","on_destroy_resource);"]
for token in require:
    assert token in src,token
assert "void try_arbitrary_sidecar" not in src
assert "DSRRL_V319_PTDE_EXACT_LERP_ENVSPEC_SLOTS" not in src
assert "DSRRL_V320_PTDE_ALL_EXACT_ENVSPEC_SLOTS" not in src
print("V323 exact named native resource SRV transport source guard PASS; pixel OPEN")
