#!/usr/bin/env python3
"""V297 source contract: exact PTDE authored slot0, not generic P_Metal."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[2]
gate=(ROOT/"renderer-core/include/dsrrl/runtime/v297_authored_slot0_material.hpp").read_text("utf-8")
donors=(ROOT/"renderer-core/data/provenance/ptde_material_donor_registry.hpp").read_text("utf-8")
router=(ROOT/"renderer-core/include/dsrrl/operators/material_response/generated_envspec_router_v1.hpp").read_text("utf-8")
integrated=(ROOT/"renderer-core/integrated/integrated_addon.cpp").read_text("utf-8")
names={
 "P_DullLeather[DSB].mtd":("7ea7d6ce9bb7148db62e8c27ee45edefb9547018d0b723dd4753d4c37e29f293",2.0),
 "P_DullLeather[DSB]_Edge.mtd":("44e1188bafa99d49349883dcc2052c13365f196e341ca5e7c788cbad0a4e058d",2.0),
 "Ps_Body[DSB].mtd":("af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95",8.0),
}
assert len(re.findall(r'\{"(?:P_DullLeather\[DSB\](?:_Edge)?|Ps_Body\[DSB\])\.mtd"',gate))==3
for name,(sha,c102) in names.items():
    assert f'"{name}"' in gate
    assert sha in gate and sha in donors
    donor=re.search(r'\{"'+sha+r'",\s*\{([^}]*)\},\s*\d+,\s*\{([^}]*)\},\s*\d+,\s*([^,]+),\s*([^,]+),\s*true\}',donors)
    assert donor is not None, name
    assert all(abs(float(x.strip().rstrip("f"))-0.5)<1e-6 for x in donor.group(1).split(",")),name
    assert all(abs(float(x.strip().rstrip("f"))-1.0)<1e-6 for x in donor.group(2).split(",")),name
    assert abs(float(donor.group(3).rstrip("f"))-c102)<1e-6,name
    assert int(donor.group(4))==0,name
    start=router.index(f'"{name}"')
    assert re.search(r'envspec_router_state::present,\s*0u,',router[start:start+430]),name
for tok in ["m.actual_material_exact", "m.owner_tuple_exact", "m.material_slot_valid",
            "m.route_index != 0u", "has_exact_flver_material_ownership",
            "classify_mtd_envspec_semantics", "env.envspc_slot!=0u"]:
    assert tok in gate,tok
assert "effective_envspec_decision" in integrated
parser = (ROOT/"renderer-core/src/runtime/flver_engine_hooks.cpp").read_text("utf-8")
for token in (
    "V297_AUTHORED_SLOT0_DIAG",
    "dsrrl::runtime::v297::raw_sha_equal",
    "const auto pilot_index=extension_index-ext_count",
    "identity.actual_material_exact=true;",
    "identity.owner_tuple_exact &&",
    "identity.raw_mtd_sha256==parsed_mtd.raw_mtd_sha256",
    "identity.semantic_name_hash==parsed_mtd.semantic_name_hash",
):
    assert token in parser, token
assert "g_pmetal_envspec.prepare(" in integrated
assert "!v296_partial_material" in integrated
assert re.search(r'if \(context\) \{\s*const bool exact =',integrated), "V296 inactive MR route not guarded"
print("V297_SOURCE_CONTRACT_PASS exact_authored_rows=3 full_owner_slot_mtd=required")
print("V297_RUNTIME_AND_PIXEL_OPEN")
