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
    mr_cpp=(root/"src/operators/material_response/material_response_island.cpp").read_text(encoding="utf-8")
    owner_cpp=(root/"src/runtime/material_owner_producer.cpp").read_text(encoding="utf-8")
    policy=(root/"include/dsrrl/core/draw_transaction_policy.hpp").read_text(encoding="utf-8")
    adapter=(root/"src/runtime/island_draw_adapter.cpp").read_text(encoding="utf-8")
    transaction=(root/"src/runtime/draw_state_transaction.cpp").read_text(encoding="utf-8")

    material_router=json.loads((root/"data/census/ptde_mtd_envspec_router_v1.json").read_text(encoding="utf-8"))
    material_records=material_router.get("records",[])
    nospc_materials=[
        r for r in material_records
        if r.get("state")=="NOSPC_HOST"
    ]
    if len(material_records)!=325:
        fail(f"expected 325 exact PTDE/DSR material pairs, got {len(material_records)}")
    if len(nospc_materials)!=25:
        fail(f"expected 25 exact NoSpc material authorities, got {len(nospc_materials)}")
    if any(r.get("homology_class")!="HOMOLOGOUS_NOSPC" for r in nospc_materials):
        fail("NoSpc material authority contains a non-homologous pair")
    if any(r.get("dsr_spx")!=r.get("ptde_spx") for r in nospc_materials):
        fail("NoSpc material authority contains a non-exact SPX pair")
    if any(r.get("c102_f32_bits")!="0x00000000" for r in nospc_materials):
        fail("NoSpc material authority unexpectedly carries PTDE c102")
    if any(any(v!="0x00000000" for v in r.get("c101_f32_bits",[])) for r in nospc_materials):
        fail("NoSpc material authority unexpectedly carries PTDE c101")
    if any(len(r.get("c100_f32_bits",[]))!=3 for r in nospc_materials):
        fail("NoSpc material authority is missing exact PTDE c100 bits")
    semantic_hashes=[r.get("semantic_name_hash_fnv1a_utf8") for r in nospc_materials]
    exact_pairs=[
        (r.get("semantic_name_hash_fnv1a_utf8"),r.get("dsr_mtd_sha256"))
        for r in nospc_materials
    ]
    if len(set(semantic_hashes))!=25 or len(set(exact_pairs))!=25:
        fail("NoSpc exact-name/raw-MTD authority is ambiguous")

    require(sidecar_h,"std::uint32_t material_max_pnt_lit_num","material-limit ABI")
    if "material_max_pnt_lit_num > 4u" in sidecar_cpp or "material_max > 4u" in draw_cpp:
        fail("clustered material limit still has an artificial >4 fail-open")
    require(sidecar_cpp,"b12[3][0]","effective-count carrier")
    require(draw_cpp,"g_retained_selector","retained selector")
    require(draw_cpp,"mirror_first_four","independent first-four mirror")
    require(draw_cpp,"capture_source","raw source capture")
    require(draw_cpp,"executable_address(target)","source vfunc executable gate")
    require(draw_cpp,"0x55FC70u","retained selector RVA")
    require(flver_cpp,"k_builder=0x22084Fu","ordinary builder hook")
    require(flver_cpp,"builder_armed=true","builder hook attestation")

    # Exact PTDE NoSpc material authority is intentionally direct-PointLight
    # only: authenticated FLVER+slot owner -> exact semantic name/raw-MTD pair
    # -> bit-exact PTDE c100. It must not expand the generic MR route cohort.
    require(owner_cpp,"resolve_exact_direct_nospc_raw_mtd","NoSpc owner raw-MTD authority")
    require(owner_cpp,"envspec_router_state::nospc_host","NoSpc owner class gate")
    require(owner_cpp,"record.semantic_name_hash","NoSpc owner semantic gate")
    require(mr_cpp,"resolve_direct_nospc_authority","direct NoSpc material resolver")
    require(mr_cpp,"record.raw_mtd_sha256","direct NoSpc raw-MTD gate")
    require(mr_cpp,"f32_from_bits","bit-exact PTDE c100 decode")
    require(mr_cpp,"if (require_legacy_specular)","NoSpc specular exclusion")
    require(integrated,"evaluate_direct_pointlight_material","direct PointLight material resolver call")
    require(integrated,"direct_pointlight_requires_specular","receiver-derived specular requirement")

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
    require(policy,"draw_additional_owner_masks_valid","pure ownership validation contract")
    require(adapter,"draw_additional_owner_masks_valid","production adapter ownership contract")
    require(policy,"{operator_id::point_light, draw_transaction_mode::draw_required","PointLight draw transaction")
    require(policy,"{operator_id::local_specular_legacy, draw_transaction_mode::draw_required","legacy spec draw transaction")

    # Final transaction closure: shader+b12+t18+t19 mutation is replayed only
    # through the shared draw transaction and all captured D3D11 state is
    # restored (and optionally read back) before the host draw path resumes.
    require(adapter,"transactions.replay_draw(","non-indexed shared draw transaction")
    require(adapter,"transactions.replay_draw_indexed(","indexed shared draw transaction")
    require(transaction,"PSSetShader(","pixel-shader mutation/restore")
    require(transaction,"restore_ps_constant_buffer_window","constant-buffer restore")
    require(transaction,"PSSetShaderResources(","SRV mutation/restore")
    require(transaction,"return restore(cmd_list, state)","post-draw restore gate")
    require(transaction,"issued_restore_failed","restore failure quarantine path")

    print("Clustered PntS pre-runtime activation source audit: PASS")
    print("  receivers=36 spc=24 nospc=12 stock_membership=t16/t17_bypassed")
    print("  producer=builder+retained_selector+independent_mirror+raw_source")
    print("  carrier=b12[3].x+t18+t19 material_max=uint32 min_after_first4")
    print("  material=25 exact HOMOLOGOUS_NOSPC name+raw-MTD pairs with bit-exact PTDE c100")
    print("  chain=candidate>receiver>selector>sources>material>sidecar>shader>draw-mutation>restore")
    print("  draw=adapter-valid ownership + exact pipeline route + shared transaction")
    return 0

if __name__=="__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Clustered PntS pre-runtime activation source audit: FAIL: {exc}",file=sys.stderr)
        raise SystemExit(1)
