#!/usr/bin/env python3
"""V361 post-build materialization source contract: identity witness only."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
cpp=(root/"integrated/integrated_addon.cpp").read_text("utf-8")
v360=(root/"tools/test_v360_fixed_equipment_pointlight.py").read_text("utf-8")
v361=(root/"tools/materialize_v361_fixed_bind_census.py").read_text("utf-8")
build=(root/"tools/build_v361_fixed_bind_census.ps1").read_text("utf-8")
def check(test,why):
    if not test: raise AssertionError("V361 source contract: "+why)
for marker in (
  "#if defined(DSRRL_V361_FIXED_BIND_CENSUS)",
  "k_v361_fixed_slots = 4096u;",
  "v361_register_fixed_handle(pipeline.handle);",
  "v361_forget_fixed_handle(pipeline.handle);",
  "v361_is_fixed_handle(pipeline.handle);",
  "v361_log_bind_witness(",
  "g_v361_fixed_handle_nonpixel_bind.fetch_add(",
  "g_v361_fixed_handle_route_missing.fetch_add(",
  "g_v361_fixed_route_unknown_handle.fetch_add(",
  "g_v361_fixed_forward_calls.fetch_add(",
  "g_v361_fixed_forward_ok.fetch_add(",
  "v361_report_bind_witness(tag);",
  "[DSRRL V361 FIXED BIND] event=exact_pipeline_init",
  "[DSRRL V361 FIXED BIND] event=exact_handle_bind",
  "[DSRRL V361 FIXED BIND] tag=%s",
  "[DSRRL V360 FIXED EQUIPMENT] stage=draw_issued",
  "constexpr bool k_clustered_pointlight_runtime_enabled = false;",
  "if (k_clustered_pointlight_runtime_enabled) {",
  "prepare_fixed_pointlight_material_requests(",
):
    check(marker in cpp,"missing "+marker)
check("g_v361_fixed_pipeline_handles[" in cpp,"explicit immutable handle witness")
check("compare_exchange_strong(" in cpp,"exact probe insert and destroy")
check("for (std::size_t i=0;i<8u;++i)" in cpp,"bounded exact collision lookup")
check("if (!exact_handle) return;" in cpp,"nonfixed drawing leaves source unchanged")
check("v361_register_fixed_handle(pipeline.handle);" in cpp,
      "only on_init exact result records handle")
check("v361_forget_fixed_handle(pipeline.handle);" in cpp,
      "never promote destroyed handle")
check("v361_reset_bind_witness();" in cpp,"reboot reset")
check(cpp.count("g_fixed_pointlight_pipeline.on_bind_pipeline(")==1,
      "fixed binding still exactly one original call")
check(cpp.count("const bool fixed_in_batch =\n        prepared.fixed_in_batch;")==2,
      "V360 indexed and regular draw telemetry preserved")
check("g_v361_init_log_budget.fetch_add(" in cpp and "if (budget<8u)" in cpp,
      "init logs bounded")
check("g_v361_bind_log_budget.fetch_add(" in cpp and "if (budget<12u)" in cpp,
      "bind logs bounded")
check("hashing::sha256(" in cpp,"SHA witness")
for marker in (
    "build_v350_gpu_resident_pointlight_off_rc2.ps1",
    "materialize_v359_attested_gi_name_alias.py",
    "materialize_v360_fixed_equipment_pointlight.py",
    "test_v360_fixed_equipment_pointlight.py",
    "materialize_v361_fixed_bind_census.py",
    "test_v361_fixed_bind_census.py",
    "'-DDSRRL_V360_FIXED_EQUIP_TEST=ON'",
    "'-DDSRRL_V361_FIXED_BIND_CENSUS=ON'",
    "/DDSRRL_V361_FIXED_BIND_CENSUS",
    "DSRRL_V361_FIXED_BIND_CENSUS.addon64",
): check(marker in build,"build lineage incomplete: "+marker)
check("update material" not in v361.lower(),"must not widen sidecars")
check("const bool k_clustered_pointlight_runtime_enabled = false;" in v360,
      "keep V360 policy")
print("V361_EXACT_FIXED_INIT_BIND_ROUTE_IDENTITY_SOURCE_PASS; runtime_pixel_OPEN")
