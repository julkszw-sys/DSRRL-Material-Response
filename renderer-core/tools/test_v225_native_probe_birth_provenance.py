#!/usr/bin/env python3
"""V225 fail-open and diagnostic source contract."""
import importlib.util
from pathlib import Path
p=Path(__file__).with_name("apply_v225_native_probe_birth_provenance.py")
sp=importlib.util.spec_from_file_location("v225",p)
v=importlib.util.module_from_spec(sp)
sp.loader.exec_module(v)
assert "v225_record_birth" in v.GLOBAL_INSERT
assert "g_v225_birth.clear()" in v.RESET_REPLACE
assert "g_v225_birth.erase" in v.DESTROY_REPLACE
assert "initial_data == nullptr ? 1u : 2u" in v.INIT_REPLACE
assert "birth_sha256" in v.HEADER_INSERT
assert "V225 PROBE ORIGIN" in v.DRAW_REPLACE
assert "stock_dsr_if_unresolved=1" in v.DRAW_REPLACE
assert "get_ptde_cube" not in v.DRAW_REPLACE
assert "env_resource_ready = " not in v.DRAW_REPLACE
print("V225_NATIVE_PROBE_BIRTH_SHA_ORIGIN_ORACLE_PASS")
print("V225_NO_NEW_PROBE_AUTHORITY_PMETAL_UNCHANGED_PASS")
