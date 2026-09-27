#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
import sys

def fail(msg):
    raise RuntimeError(msg)

def require(text, needle, label):
    if needle not in text:
        fail(f"{label}: missing {needle!r}")

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--source-dir",required=True)
    ns=ap.parse_args()
    root=Path(ns.source_dir).resolve()

    journal=json.loads((root/"data/provenance/clustered_pnts_direct_stock_journal_v1.json").read_text(encoding="utf-8"))
    if journal.get("schema")!="DSRRL_CLUSTERED_PNTS_DIRECT_PTDE_FOUR_SLOT_STOCK_JOURNAL_V1":
        fail("clustered PntS journal schema is not the four-slot bypass schema")
    entries=journal.get("entries",[])
    if len(entries)!=36:
        fail(f"expected 36 clustered PntS plans, got {len(entries)}")
    if any(not e.get("stock_cluster_membership_bypassed",False) for e in entries):
        fail("at least one clustered PntS plan still owns stock membership")
    spc=sum(e.get("material_class")=="Spc" for e in entries)
    nospc=sum(e.get("material_class")=="NoSpc" for e in entries)
    if (spc,nospc)!=(24,12):
        fail(f"unexpected clustered receiver partition {(spc,nospc)}")

    sidecar_h=(root/"include/dsrrl/operators/point_light/clustered_sidecar.hpp").read_text(encoding="utf-8")
    sidecar_cpp=(root/"src/operators/point_light/clustered_sidecar.cpp").read_text(encoding="utf-8")
    draw_cpp=(root/"src/runtime/clustered_pnts_draw_runtime.cpp").read_text(encoding="utf-8")
    pipe_cpp=(root/"src/runtime/clustered_pnts_pipeline_runtime.cpp").read_text(encoding="utf-8")
    flver_cpp=(root/"src/runtime/flver_engine_hooks.cpp").read_text(encoding="utf-8")
    integrated=(root/"integrated/integrated_addon.cpp").read_text(encoding="utf-8")
    policy=(root/"include/dsrrl/core/draw_transaction_policy.hpp").read_text(encoding="utf-8")

    require(sidecar_h,"std::uint32_t material_max_pnt_lit_num","material-limit ABI")
    if "material_max_pnt_lit_num > 4u" in sidecar_cpp or "material_max > 4u" in draw_cpp:
        fail("clustered material limit still has an artificial >4 fail-open")
    require(sidecar_cpp,"b12[3][0]","effective-count carrier")
    require(draw_cpp,"g_retained_selector","retained selector")
    require(draw_cpp,"mirror_first_four","independent first-four mirror")
    require(draw_cpp,"capture_source","raw source capture")
    require(draw_cpp,"0x55FC70u","retained selector RVA")
    require(flver_cpp,"k_builder=0x22084Fu","ordinary builder hook")
    require(flver_cpp,"builder_armed=true","builder hook attestation")

    if "clustered.additional_owners =\n                point | mr | local_if_spc;" in integrated:
        fail("clustered request duplicates primary in additional_owners")
    if "clustered.additional_resource_owners =\n                point | local_if_spc;" in integrated:
        fail("clustered resource ownership duplicates primary")
    require(integrated,"? (point | mr)","Spc additional ownership")
    require(integrated,": mr;","NoSpc additional ownership")
    require(integrated,"12u,\n                prepared.clustered_carrier.b12","clustered b12 slot")
    require(integrated,"18u,\n                prepared.clustered_carrier.t18","clustered t18 slot")
    require(integrated,"19u,\n                prepared.clustered_carrier.t19","clustered t19 slot")
    require(integrated,"k_route_clustered_pointlight","clustered route bit")
    require(integrated,"dsrrl::core::operator_id::point_light,","full PointLight feature enable")
    require(integrated,"dsrrl::core::operator_id::local_specular_legacy,","legacy spec feature enable")
    require(integrated,"g_clustered_pnts.install()","clustered producer install")
    require(integrated,"g_clustered_pnts_pipeline.on_init_pipeline","clustered pipeline attestation")
    require(integrated,"g_clustered_pnts_pipeline.on_bind_pipeline","clustered pipeline bind")

    a1=integrated.find("g_a1_bridge.on_create_pipeline")
    reg=integrated.find("g_clustered_pnts_pipeline.register_candidate")
    if a1<0 or reg<0 or not a1<reg:
        fail("clustered candidate is not registered against the post-A1 host")

    require(pipe_cpp,"replacement_sha256","replacement shader attestation")
    require(pipe_cpp,"pipeline_attested","pipeline attestation registry")
    require(policy,"{operator_id::point_light, draw_transaction_mode::draw_required","PointLight draw transaction")
    require(policy,"{operator_id::local_specular_legacy, draw_transaction_mode::draw_required","legacy spec draw transaction")

    print("Clustered PntS pre-runtime activation source audit: PASS")
    print("  receivers=36 spc=24 nospc=12 stock_membership=t16/t17_bypassed")
    print("  producer=builder+retained_selector+independent_mirror+raw_source")
    print("  carrier=b12[3].x+t18+t19 material_max=uint32 min_after_first4")
    print("  draw=adapter-valid ownership + exact pipeline route + shared transaction")
    return 0

if __name__=="__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Clustered PntS pre-runtime activation source audit: FAIL: {exc}",file=sys.stderr)
        raise SystemExit(1)
