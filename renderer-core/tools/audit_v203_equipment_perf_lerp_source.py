#!/usr/bin/env python3
"""Read-only v2.0.3 PR300 performance/Lerp/m10/EnvDiffuse source contract.
Outputs structural work and safety topology, NOT a real game frame-time sample.
Never mutates the renderer, sources, generated blobs or binary.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[2]
R = ROOT / "renderer-core"
def source(path):
    return (R / path).read_text(encoding="utf-8")

data = {
    "manifest": source("include/dsrrl/runtime/v203_equipment_flver_slot_spec_manifest.hpp"),
    "gate": source("include/dsrrl/runtime/v203_equipment_envspec36_gate.hpp"),
    "selector": source("src/runtime/flver_engine_hooks.cpp"),
    "source_donor": source("data/provenance/r43_zero_selector_hybrid_source.cpp"),
    "source_overlay": source("tools/apply_v203_envspec36_generated_source.py"),
    "resource_overlay": source("tools/apply_v203_clean_equipment_spec.py"),
    "draw": source("src/runtime/pmetal_envspec_draw_runtime.cpp"),
    "resources": source("src/runtime/envspec_resource_runtime.cpp"),
    "material_resources": source("src/runtime/material_resource_draw_runtime.cpp"),
    "lerp_shader": source("src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp"),
    "policy": source("include/dsrrl/runtime/pmetal_selector_policy.hpp"),
    "producer": source("src/runtime/pmetal_producer_state.cpp"),
    "semantic": source("src/operators/material_response/mtd_semantic_census.cpp"),
    "cxx": source("integrated/CMakeLists.txt"),
    "build": source("tools/configure_v203_clean_equipment.ps1"),
    "existing_test": source("tools/test_v203_envspec36_offline_port.py"),
    "integrated": source("integrated/integrated_addon.cpp"),
}
def require(where,needle):
    if needle not in data[where]:
        raise AssertionError(where+": missing "+needle[:100])
    return True

rows = re.findall(r'\{ "([a-f0-9]{64})", (\d+)u, "([^"]+)", "([^"]+)" \},', data["manifest"])
if len(rows) != 36 or len(set(rows)) != 36:
    raise AssertionError("36 exact equipment sample changed")
counts = {name: sum(1 for r in rows if r[2] == name) for name in
          ["P_DullLeather[DSB].mtd","P_Leather[DSB].mtd","P_Metal[DSB].mtd"]}
if counts != {key:12 for key in counts}:
    raise AssertionError(("unexpected material ownership",counts))
for where, needle in [
    ("gate", "exact_proven_owner_slot(query)"),
    ("gate", 'mtd_semantic_hash("P_Metal[DSB].mtd")'),
    ("gate", "material.route_index == 345u"),
    ("gate", "classify_mtd_envspec_semantics(query)"),
    ("gate", "env.envspc_slot < 4u"),
    ("gate", "decision.receiver_id >= 33u"),
    ("gate", "decision.receiver_id <= 35u"),
    ("selector","exact_envspec36_material(identity)"),
    ("source_overlay","exact_envspec36_or_pmetal_source"),
    ("source_overlay","if (!exact_pmetal_material_selection(material))"),
    ("source_overlay","GENERATOR_NONE_IMPOSSIBLE"),
]:
    if needle != "GENERATOR_NONE_IMPOSSIBLE":
        require(where,needle)
for where, needle in [
    ("source_donor","pmetal_selector_policy::is_synthetic_zero_descriptor("),
    ("policy","ret_rva == 0x20EB7Fu && raw_a == 0u"),
    ("policy","raw_b == 0u && beta == 0.0f"),
    ("resource_overlay","exact_equipment_envspec_consumer"),
    ("resource_overlay","exact_proven_slot_spec_hash(query, h1)"),
    ("resource_overlay","evaluate_spec_rgb_route(context_spec)"),
    ("resource_overlay","evaluate_spec_rgb_route(context_spec, query)"),
    ("lerp_shader","apply_exact_ptde_envdiffuse_lerp_consumer("),
    ("lerp_shader","outcome.envdiffuse_preserved = false;"),
    ("draw","outcome.envdiffuse_preserved ||"),
    ("draw","ptde_envdiffuse_b_required"),
    ("draw","env_resources_.prepare_envdiffuse("),
    ("draw","prepared.envdiffuse_resources.probe_a"),
    ("resources","bool envspec_resource_runtime::prepare_envdiffuse("),
    ("resources","get_ptde_envdiffuse_cube("),
    ("resources","g_ptde_envdiffuse_cubes"),
    ("resources","g_resource_by_view.find(key)"),
    ("resources","g_envdiffuse_pack_ready"),
    ("integrated","hemenvlerp_bound"),
    ("integrated","g_pmetal_envspec.prepare("),
    ("integrated","SpecRGB consumer"),
    ("cxx","DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG"),
    ("build","DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE=ON"),
    ("build","DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL=ON"),
]:
    require(where,needle)

# The historical exact release donor stays unmodified, and the new producer
# patch applies only AFTER it reconstructs the runtime from pinned authority.
target = (R / "src/runtime/pmetal_env_source_runtime.cpp").read_bytes().replace(b"\r\n", b"\n")
git_blob = hashlib.sha1(b"blob "+str(len(target)).encode()+b"\0"+target).hexdigest()
if git_blob != "a6904844c97c712027ad79bbcd136066952a7116":
    raise AssertionError("R43 pinned source input was edited; abort audit")
require("source_overlay","[DSRRL PMETAL ZERO RE]")
require("source_overlay","v203_equipment_envspec36_gate.hpp")

# Enumerate mechanical costs and candidate frontiers without asserting how
# many draw calls the user's PC executes or inventing nanoseconds.
matrix = [
  {"site":"exact nonmetal material authority","frequency":"selector and eligible draw","work":"linear 36-entry SHA/slot/MTD proof; raw MTD scan up to 325 records","risk":"low semantic risk, potentially repeated cost","action":"profile hit/miss before immutable-index experiment"},
  {"site":"FLVER source producer","frequency":"eligible selector callback","work":"descriptor classification, LightBank A/B, keyed 256-slot state; producer may lock","risk":"high semantic risk: synthetic zero + cross-thread epochs","action":"freeze producer/m10 semantics; sample exclusive stage times"},
  {"site":"EnvSpec draw preparation","frequency":"eligible draw","work":"exact family/MTD + source lookup, receptor checks","risk":"extra rejected draws cost; Lerp state changes","action":"sample counts by rx33/34/35 stable vs paired Lerp"},
  {"site":"PTDE sidecar lookup","frequency":"eligible draw","work":"inspect_many three SRVs; epoch/class/name/companion lookup; 36-row gSpec verification","risk":"resource identity and independent roughness alpha must survive","action":"sample hash hit/miss and CPU time, never drop checks blindly"},
  {"site":"native probes","frequency":"eligible draw","work":"PSGetShaderResources t12/t14, map lookup and mutex, AddRef/Release","risk":"D3D11 state and lifetime; shared downstream t13","action":"sample cache hit/lock wait outside GPU critical path"},
  {"site":"EnvDiffuse sidecar RGBA","frequency":"eligible stable or Lerp draw","work":"reuse probe ordinal, first-use GPU cube materialization and cached SRVs; Lerp adds B cube","risk":"cold spikes, pack/slot mismatch, shared resource reuse","action":"separate first-use misses from steady-state; no new loader"},
  {"site":"constant-buffer b12","frequency":"eligible draw","work":"per-context mutex/cache + Map WRITE_DISCARD/Unmap","risk":"synchronization and GPU stall; not quantified","action":"record Map time, buffer cache hit and context"},
  {"site":"transaction and state restore","frequency":"eligible draw","work":"shader/CB/SRV/sampler bind + full native state restore","risk":"GPU pipeline and CPU state traffic","action":"sample transaction capture/mutate/restore separately"},
]
out={
    "kind":"OFFLINE_STRUCTURAL_PERFORMANCE_AUDIT_NOT_RUNTIME_BENCHMARK",
    "baseline_pr":300,
    "manifest_rows":len(rows),
    "nonmetal_owner_rows":24,
    "metal_original_rows":12,
    "unique_flver_sha256":len({x[0] for x in rows}),
    "unique_logical_spec_names":len({x[3] for x in rows}),
    "raw_mtd_router_records":325,
    "source_pinned_git_blob_sha1":git_blob,
    "m10_row0":"existing synthetic-zero source guard inherited; never ban true row0",
    "lerp":"R19 paired shader/registration and beta-t11-t13 structural wiring present; nonmetal runtime and pixel open",
    "envdiffuse":"existing P_Metal canonical probe-ordinal A/B pack reused; cold and steady costs separate; nonmetal runtime open",
    "performance_status":"unmeasured on target machine; historical launch-dependent regression cannot be assigned to this bridge",
    "mutates_game_or_addon":False,
    "performance_frontiers":matrix
}
ap=argparse.ArgumentParser()
ap.add_argument("--json-out",type=Path)
args=ap.parse_args()
if args.json_out:
    args.json_out.parent.mkdir(parents=True,exist_ok=True)
    args.json_out.write_text(json.dumps(out,indent=2)+"\n",encoding="utf-8")
print(json.dumps({k:v for k,v in out.items() if k!="performance_frontiers"},indent=2))
print("V203_PERF_LERP_ENVDIFFUSE_SOURCE_ONLY_PASS: 8 frontiers; runtime/pixel OPEN")
