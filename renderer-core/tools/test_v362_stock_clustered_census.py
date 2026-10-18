#!/usr/bin/env python3
"""V362 exact stock PntS source contract; no PointLight activation claims."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
cpp=(root/"integrated/integrated_addon.cpp").read_text("utf-8")
build=(root/"tools/build_v362_stock_clustered_census.ps1").read_text("utf-8")
check=lambda cond,what: cond or (_ for _ in ()).throw(AssertionError("V362 "+what))
for marker in (
  "constexpr bool k_clustered_pointlight_runtime_enabled = false;",
  "v361_register_fixed_handle(pipeline.handle);",
  "v361_is_fixed_handle(pipeline.handle);",
  "v361_report_bind_witness(tag);",
  "k_v362_slots=4096u;",
  "local_specular_receiver_for_shader(",
  "local_specular_receiver_class::clustered_spc_pnts)",
  "v362_register(pipeline.handle);",
  "v362_forget(pipeline.handle);",
  "v362_bind(cmd_list,pipeline.handle,pixel_stage_bound,route_mask);",
  "v362_draw(cmd_list,false);",
  "v362_draw(cmd_list,true);",
  "v362_snapshot(tag);",
  "v362_reset();",
  "[DSRRL V362 STOCK PNTS] event=exact_cluster_bind",
  "[DSRRL V362 STOCK PNTS] tag=%s",
  "clustered_bridge=DISABLED stock=UNCHANGED pixel=OPEN",
): check(marker in cpp,"missing "+marker)
check(cpp.count("v362_draw(cmd_list,false);")==1,"regular draw exactly once")
check(cpp.count("v362_draw(cmd_list,true);")==1,"indexed draw exactly once")
check(cpp.count("g_clustered_pnts_pipeline.on_bind_pipeline(")==1,"clustered binding path preserved")
check(cpp.count("g_fixed_pointlight_pipeline.on_bind_pipeline(")==1,"fixed binding path preserved")
for marker in (
  "materialize_v359_attested_gi_name_alias.py",
  "materialize_v360_fixed_equipment_pointlight.py",
  "materialize_v361_fixed_bind_census.py",
  "materialize_v362_stock_clustered_census.py",
  "test_v361_fixed_bind_census.py",
  "test_v362_stock_clustered_census.py",
  "/DDSRRL_V362_STOCK_CLUSTERED_BIND_CENSUS",
  "'-DDSRRL_V362_STOCK_CLUSTERED_BIND_CENSUS=ON'",
  "DSRRL_V362_STOCK_PNTS_CENSUS.addon64"
): check(marker in build,"missing ancestor/source "+marker)
print("V362_PASS exact stock clustered witness only; activation_PIXEL_OPEN")
