#!/usr/bin/env python3
"""Conservative regression contract for V230 atomic island separation.

Construction-only tests; they are not a runtime or PTDE-pixel verdict.
The gate may rescue certified stock-t2 -> PTDE-normal ONLY after the atomic
EnvSpec operator failed. All working atomic paths remain bitwise unchanged.
"""
from pathlib import Path
import ast

src=Path("renderer-core/tools/materialize_v230_nonmetal_operator_split.py").read_text("utf-8")
ast.parse(src)
required=(
    "V230_NONMETAL_NORMAL_SPLIT_OPERATOR_LOCAL",
    "material.route_index != 345u",
    '"P_Metal[DSB].mtd"',
    '"Ps_Body[DSB].mtd"',
    '"Ps_Body[DSB][M].mtd"',
    '"Ps_Body[DSB]_Alp.mtd"',
    "[DSRRL V230 NORMAL SPLIT]",
    "!hemenvlerp_bound",
    "material.owner_tuple_exact",
    "material.actual_material_exact",
    "material.material_slot_valid",
    "false, false, independent",
    "independent.request_count == 1u",
    "independent.normal",
    "!independent.spec_rgb",
    "!independent.diffuse",
    "operator_id::normal",
    "independent.requests[0].srvs[0].slot == 2u",
    "!independent.requests[0].replace_pixel_shader",
    "independent.requests[0].constant_buffer_count == 0u",
    "release_prepared_island_batch(prepared)",
    "g_material_resources.release_prepared_draw(independent)",
)
for token in required:
    assert token in src, ("MISSING_CONTRACT",token)

def route(atomic_ok, stable, owner, actual, slot, valid,
          route_index, is_pmetal, is_body, receiver, normal_ready):
    if atomic_ok:
        return "original_atomic"
    if not (stable and owner and actual and slot and valid and
            route_index!=345 and not is_pmetal and not is_body and
            33<=receiver<=35):
        return "stock_fail_open"
    if not normal_ready:
        return "stock_fail_open"
    return "normal_t2_only"

default=dict(atomic_ok=False,stable=True,owner=True,actual=True,
             slot=True,valid=True,route_index=160,is_pmetal=False,
             is_body=False,receiver=34,normal_ready=True)
def check(expect,**overrides):
    v={**default,**overrides}
    got=route(**v)
    assert got==expect,(expect,got,v)

# Protected positives cannot change, even if the normal sidecar exists.
check("original_atomic",atomic_ok=True)
check("original_atomic",atomic_ok=True,route_index=345,is_pmetal=True)
check("original_atomic",atomic_ok=True,stable=False)
check("stock_fail_open",route_index=345)
check("stock_fail_open",is_pmetal=True)
check("stock_fail_open",is_body=True,route_index=232)
check("stock_fail_open",stable=False)
check("stock_fail_open",owner=False)
check("stock_fail_open",actual=False)
check("stock_fail_open",slot=False)
check("stock_fail_open",valid=False)
check("stock_fail_open",receiver=32)
check("stock_fail_open",receiver=36)
check("stock_fail_open",normal_ready=False)
check("normal_t2_only",route_index=160)
check("normal_t2_only",route_index=88)
check("normal_t2_only",route_index=229)
print("V230_NONMETAL_OPERATOR_SPLIT_CONTRACT_PASS 17 scenario checks; pixel OPEN")
