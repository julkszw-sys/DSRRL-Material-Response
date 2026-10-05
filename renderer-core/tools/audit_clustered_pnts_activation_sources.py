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

    # The direct replacement is a composed shader payload. Audit the exact
    # create-time operator transforms in the journal instead of assuming the
    # PointLight runtime is the sole shader owner.
    for entry in entries:
        counts={
            "diffuse":0,
            "atten_a":0,
            "atten_b":0,
            "envspec_delete":0,
            "terminal_sat":0,
        }
        for op in entry.get("ops",[]):
            old=op.get("old_tokens",[])
            new=op.get("new_tokens",[])
            if old==[0x400ccccd,0x400ccccd,0x400ccccd] and new==[0x3f800000,0x3f800000,0x3f800000]:
                counts["diffuse"]+=1
            elif old==[0x07000038] and new==[0x07000033]:
                counts["atten_a"]+=1
            elif old==[0x07002038] and new==[0x07002034]:
                counts["atten_b"]+=1
            elif old==[0x07000038] and new==[0x07000031]:
                counts["envspec_delete"]+=1
            elif old==[0x05000036] and new==[0x05002036]:
                counts["terminal_sat"]+=1

        expected_env=0 if entry.get("material_class")=="Spc" else 1
        if counts!={
            "diffuse":1,
            "atten_a":1,
            "atten_b":1,
            "envspec_delete":expected_env,
            "terminal_sat":1,
        }:
            fail(f"clustered composed shader ownership drift for {entry.get('shader_name')}: {counts}")

    sidecar_h=(root/"include/dsrrl/operators/point_light/clustered_sidecar.hpp").read_text(encoding="utf-8")
    sidecar_cpp=(root/"src/operators/point_light/clustered_sidecar.cpp").read_text(encoding="utf-8")
    draw_h=(root/"include/dsrrl/runtime/clustered_pnts_draw_runtime.hpp").read_text(encoding="utf-8")
    draw_cpp=(root/"src/runtime/clustered_pnts_draw_runtime.cpp").read_text(encoding="utf-8")
    pipe_cpp=(root/"src/runtime/clustered_pnts_pipeline_runtime.cpp").read_text(encoding="utf-8")
    pipe_h=(root/"include/dsrrl/runtime/clustered_pnts_pipeline_runtime.hpp").read_text(encoding="utf-8")
    materializer_cpp=(root/"src/operators/point_light/clustered_pnts_direct_materializer.cpp").read_text(encoding="utf-8")
    materializer_h=(root/"include/dsrrl/operators/point_light/clustered_pnts_direct_materializer.hpp").read_text(encoding="utf-8")
    flver_cpp=(root/"src/runtime/flver_engine_hooks.cpp").read_text(encoding="utf-8")
    integrated=(root/"integrated/integrated_addon.cpp").read_text(encoding="utf-8")
    material_resource_cpp=(root/"src/runtime/material_resource_draw_runtime.cpp").read_text(encoding="utf-8")
    material_resource_h=(root/"include/dsrrl/runtime/material_resource_draw_runtime.hpp").read_text(encoding="utf-8")
    mr_cpp=(root/"src/operators/material_response/material_response_island.cpp").read_text(encoding="utf-8")
    owner_cpp=(root/"src/runtime/material_owner_producer.cpp").read_text(encoding="utf-8")
    policy=(root/"include/dsrrl/core/draw_transaction_policy.hpp").read_text(encoding="utf-8")
    adapter=(root/"src/runtime/island_draw_adapter.cpp").read_text(encoding="utf-8")
    transaction=(root/"src/runtime/draw_state_transaction.cpp").read_text(encoding="utf-8")
    native_draw=(root/"src/runtime/pmetal_native_draw_bridge.cpp").read_text(encoding="utf-8")

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

    supplement=json.loads((root/"data/census/dsr_mtd_identity_certified_supplement_v1.json").read_text(encoding="utf-8"))
    nospc_identity=[
        r for r in supplement.get("records",[])
        if r.get("authority_class")=="DIRECT_POINTLIGHT_NOSPC"
    ]
    if len(nospc_identity)!=25:
        fail(f"expected 25 certified direct-PointLight NoSpc identities, got {len(nospc_identity)}")
    router_identity={
        (r.get("mtd_name"),r.get("dsr_mtd_sha256"))
        for r in nospc_materials
    }
    supplement_identity={
        (r.get("semantic_name"),r.get("raw_mtd_sha256"))
        for r in nospc_identity
    }
    if supplement_identity!=router_identity:
        fail("certified NoSpc identity supplement does not exactly match the HOMOLOGOUS_NOSPC authority set")

    require(sidecar_h,"std::uint32_t material_max_pnt_lit_num","material-limit ABI")
    if "material_max_pnt_lit_num > 4u" in sidecar_cpp or "material_max > 4u" in draw_cpp:
        fail("clustered material limit still has an artificial >4 fail-open")
    require(sidecar_cpp,"b12[3][0]","effective-count carrier")

    builder_start=draw_cpp.find("void clustered_pnts_draw_runtime::builder_event(")
    selector_start=draw_cpp.find("void clustered_pnts_draw_runtime::selector_event(")
    selector_source_start=draw_cpp.find("void clustered_pnts_draw_runtime::selector_source_event()")
    selector_identity_start=draw_cpp.find("void clustered_pnts_draw_runtime::selector_identity_event(")
    authority_start=draw_cpp.find("bool clustered_pnts_draw_runtime::current_draw_authority(")
    prepare_start=draw_cpp.find("bool clustered_pnts_draw_runtime::prepare_sidecar(")
    release_start=draw_cpp.find("void clustered_pnts_draw_runtime::release_prepared_draw(")
    if min(builder_start,selector_start,selector_source_start,selector_identity_start,authority_start,prepare_start,release_start)<0:
        fail("clustered runtime stage boundaries are missing")
    if not (builder_start < selector_start < selector_source_start < selector_identity_start < authority_start < prepare_start < release_start):
        fail("clustered runtime stage ordering is invalid")

    builder_body=draw_cpp[builder_start:selector_start]
    selector_body=draw_cpp[selector_start:selector_source_start]
    selector_source_body=draw_cpp[selector_source_start:selector_identity_start]
    selector_identity_body=draw_cpp[selector_identity_start:authority_start]
    authority_body=draw_cpp[authority_start:prepare_start]
    prepare_body=draw_cpp[prepare_start:release_start]

    require(builder_body,"g_producer_input_tls","capture-only producer TLS")
    require(builder_body,"renderer_bytes + 0x2328u","collection input capture")
    require(builder_body,"draw_bytes + 0xD0u","query input capture")
    require(builder_body,"draw_bytes + 0x3Au","mask input capture")
    for forbidden,label in [
        ("select_first_four_exact(","selector traversal"),
        ("capture_source(","source capture"),
        ("readable_range(","VirtualQuery-backed range validation"),
        ("std::lock_guard","global/resource lock")
    ]:
        if forbidden in builder_body:
            fail(f"builder hot path still performs {label}")

    require(selector_body,"g_producer_input_tls","same-thread producer join")
    require(selector_body,"actual_material) + 0x384u","direct attested material-limit read")
    if "readable_range(" in selector_body or "VirtualQuery(" in selector_body:
        fail("selector join still performs OS page validation")
    if "lookup_snapshot(" in selector_body or "g_registry_mutex" in selector_body:
        fail("selector join still uses legacy synchronized producer registry")

    # R34 architecture: PTDE PointLight source production and material-response
    # authorization are separate stages. Source selection/capture is cached by
    # producer serial; the material stage may only join that exact source state
    # and materialize the receiver-specific sidecar. The draw prepare stage may
    # only consume the already materialized payload.
    require(selector_source_body,"source_cache.producer_serial != input.serial","producer-serial source cache")
    require(selector_source_body,"select_first_four_exact(","source-stage exact first-four reconstruction")
    require(selector_source_body,"capture_source(","source-stage PTDE source capture")
    require(draw_cpp,"k_frame_source_cache_entries = 128u","frame-scoped source cache capacity")
    require(draw_cpp,"g_source_frame_epoch","present-frame source cache epoch")
    require(draw_cpp,"same_frame_source_state(","exact source-object state cache gate")
    require(draw_cpp,"frame_source_cache_index(","R38 source cache direct-map index")
    require(draw_cpp,"k_frame_selection_cache_entries = 128u","R38 frame selection cache capacity")
    require(draw_cpp,"same_frame_selection_key(","R38 exact frame selection cache gate")
    require(draw_cpp,"bank_source ? 0x60u : 0x70u","attested Bank/Lerp position lane direct read")
    require(integrated,"g_clustered_pnts.frame_event(present);","present-driven source cache epoch")
    if "evaluate_direct_pointlight_material_identity(" in selector_source_body or "build_clustered_sidecar_v1(" in selector_source_body:
        fail("PointLight source stage regressed to material-response work")
    require(selector_identity_body,"evaluate_direct_pointlight_material_identity(","material-stage exact PointLight authority")
    require(selector_identity_body,"source_cache.producer_serial != input.serial","material-stage exact source join")
    require(selector_identity_body,"build_clustered_sidecar_v1(","material-stage sidecar materialization")
    if "select_first_four_exact(" in selector_identity_body or "capture_source(" in selector_identity_body:
        fail("PointLight material stage regressed to source reconstruction")
    if "std::lock_guard" in selector_source_body or "std::lock_guard" in selector_identity_body:
        fail("selector-local PointLight source/material stages must not take the GPU resource mutex")

    require(authority_body,"g_draw_selection.authority_ready","draw consumes cached PointLight authority")
    require(authority_body,"g_draw_selection.material_decision","draw reuses selector-resolved material decision")

    require(authority_body,"g_draw_selection.input.serial !=","cached authority producer-serial freshness")
    require(authority_body,"g_draw_selection.input.owner !=","cached authority producer-owner freshness")

    require(prepare_body,"material.active","authorized material gate")
    require(prepare_body,"g_draw_selection.payload_ready","draw consumes cached sidecar payload")
    require(prepare_body,"const auto &payload =","cached sidecar payload reuse")
    for forbidden,label in [
        ("select_first_four_exact(","first-four traversal"),
        ("capture_source(","source vfunc/donor capture"),
        ("build_clustered_sidecar_v1(","CPU sidecar rebuild")
    ]:
        if forbidden in prepare_body:
            fail(f"draw prepare regressed to {label}")

    require(draw_h,"neutral_no_pointlights","zero-light neutral ABI")
    require(draw_h,"prepare_neutral_empty","zero-light neutral telemetry ABI")
    require(selector_source_body,"source_cache.neutral = true;","zero-light producer-cache classification")
    require(selector_source_body,"g_draw_selection.neutral_no_pointlights = true;","zero-light draw classification")
    require(selector_source_body,"g_prepare_neutral_empty","zero-light preparation telemetry")
    require(prepare_body,"prepared.neutral_no_pointlights = true;","zero-light neutral draw handoff")
    require(integrated,"clustered_no_pointlights_neutral","zero-light neutral runtime stage")
    require(integrated,"g_clustered_draw_neutral_noop","zero-light neutral draw telemetry")
    require(integrated,"!prepared.clustered_neutral_noop","zero-light fail-open exclusion")
    require(integrated,"prepared.batch.island_count != 0u ||\n           prepared.clustered_neutral_noop","neutral-only prepare success")
    require(prepare_body,"context_type == D3D11_DEVICE_CONTEXT_DEFERRED","deferred-context activation telemetry")
    require(draw_cpp,"D3D11_USAGE_DYNAMIC","dynamic clustered sidecar resources")
    require(draw_cpp,"D3D11_MAP_WRITE_DISCARD","deferred-compatible sidecar upload")
    ensure_start=draw_cpp.find("bool ensure_gpu_locked(")
    update_start=draw_cpp.find("bool update_buffer(",ensure_start)
    if ensure_start<0 or update_start<0:
        fail("clustered GPU preparation boundaries are missing")
    ensure_body=draw_cpp[ensure_start:update_start]
    if "context->GetType() !=" in ensure_body or "D3D11_DEVICE_CONTEXT_IMMEDIATE)" in ensure_body:
        fail("clustered sidecar still has an immediate-context-only activation gate")
    require(draw_cpp,"std::unordered_map<ID3D11DeviceContext *,gpu_resources>","per-recording-context clustered GPU carrier")
    require(draw_cpp,"g_gpu_by_context.try_emplace(","context-keyed clustered GPU realization")
    if "gpu_resources g_gpu{}" in draw_cpp:
        fail("clustered sidecar regressed to a process-global dynamic GPU carrier")
    if prepare_body.count("std::lock_guard<std::mutex> lock(")!=1:
        fail("clustered sidecar must use exactly one GPU carrier synchronization point per prepared draw")
    require(draw_cpp,"upload_identity_for(","TLS upload identity cache")
    require(draw_cpp,"gpu_generation","GPU carrier generation key")
    require(prepare_body,"!upload_t18 ||","t18 upload dedupe")
    require(prepare_body,"!upload_t19 ||","t19 upload dedupe")
    require(prepare_body,"!upload_b12 ||","b12 upload dedupe")
    require(draw_cpp,"DSRRL_CLUSTERED_SELECTOR_RUNTIME_CROSSCHECK","optional retained-selector cross-check gate")
    require(draw_cpp,"g_retained_selector","retained selector available only for optional cross-check")
    require(draw_cpp,"spatial_overlap_xyz_unchecked","single node-range validation overlap path")
    require(draw_cpp,"executable_address(target)","source vfunc executable gate")
    require(draw_cpp,"0x55FC70u","retained selector RVA for optional audit cross-check")
    if "mirror_first_four(" in draw_cpp:
        fail("production clustered selector still contains the legacy double-traversal mirror path")
    if "g_registry_mutex" in draw_cpp or "lookup_snapshot(" in draw_cpp or "publish_snapshot(" in draw_cpp:
        fail("legacy globally locked clustered producer registry is still present")
    require(flver_cpp,"k_builder=0x22084Fu","ordinary builder hook")
    require(flver_cpp,"bool install(\n    bool enable_clustered_builder,\n    bool enable_upper_lower_selector,\n    bool enable_hemdir3_selector) noexcept","builder hook policy ABI")
    require(flver_cpp,"if(enable_clustered_builder){","builder preparation policy gate")
    require(flver_cpp,"(enable_clustered_builder&&!arm(g_b))","builder arm policy gate")
    require(flver_cpp,"g_state.builder_armed=enable_clustered_builder;","builder hook attestation")
    require(integrated,"flver_identity_transport::install(\n            k_pointlight_drawtime_runtime_enabled,\n            upper_lower_enabled,\n            hemdir3_enabled)","integrated builder policy routing")

    # Direct PointLight owns its dedicated 205-row exact material authority
    # (180 Spc + 25 NoSpc). Generic MR profile coverage must not decide whether
    # the PointLight operator has c100/c101/c102 authority.
    require(owner_cpp,"dsr_mtd_identity_supplement_resolve(semantic_hash, raw_mtd_sha)","certified owner raw-MTD transport")
    if "generated_envspec_router_v1.hpp" in owner_cpp or "k_envspec_router_v1" in owner_cpp:
        fail("material owner producer must not use EnvSpec/SPX router as raw-MTD identity fallback")
    require(mr_cpp,"generated_pointlight_material_authority_v1.hpp","dedicated PointLight material authority include")
    require(mr_cpp,"resolve_direct_pointlight_authority","direct PointLight material resolver")
    require(mr_cpp,"identity.actual_material_exact &&","runtime-MTD-only PointLight raw-SHA gate")
    require(mr_cpp,"identity.raw_mtd_sha256 !=","exact PointLight runtime raw-MTD gate")
    if "generated_pointlight_material_authority_v1.hpp" in owner_cpp:
        fail("generic material owner producer must not inherit PointLight-local raw-MTD authority")
    require(mr_cpp,"pointlight_material_mode::spc","Spc/NoSpc PointLight mode gate")
    require(mr_cpp,"record.c101_scalar","RGB c101 fail-open guard")
    require(mr_cpp,"f32_from_bits","bit-exact PTDE PointLight constants decode")
    require(mr_cpp,"record_spc != require_legacy_specular","receiver-derived Spc/NoSpc exclusion")
    require(mr_cpp,"direct_pointlight_material_candidate","cheap PointLight material prefilter")
    require(flver_cpp,"direct_pointlight_material_candidate(","PointLight prefilter before clustered selector runtime")
    require(flver_cpp,"if (!pointlight_spc) {\n            clustered_pnts_selector_source_event_bridge();","R39 clustered NoSpc-only source-stage dispatch")
    require(flver_cpp,"clustered_pnts_selector_identity_event_bridge(\n                identity,\n                false);","R39 NoSpc consumes produced PTDE source state")
    require(flver_cpp,"[DSRRL POINTLIGHT R39] clustered_spc_source_capture=FAIL_OPEN_STOCK_DSR","R39 Spc source-capture fail-open marker")
    require(integrated,"g_clustered_pnts.current_draw_authority(","clustered draw cached authority lookup")
    require(integrated,"clustered_pointlight_spc","receiver-derived clustered Spc requirement")
    require(integrated,"[DSRRL POINTLIGHT GATE]","one-shot direct PointLight rejection trace")
    require(integrated,"clustered_metadata_unbound","pipeline-route versus bound-metadata rejection trace")
    clustered_marker=integrated.find("// Clustered PntS owns PTDE first-four membership")
    clustered_begin=integrated.find("if (clustered_pointlight_bound &&",clustered_marker)
    fixed_begin=integrated.find("// Fixed PntSS/PntSSSS is a separate exact receiver namespace.",clustered_begin)
    if clustered_marker<0 or clustered_begin<0 or fixed_begin<0 or not clustered_begin<fixed_begin:
        fail("clustered/fixed integrated branch boundaries are missing")
    clustered_block=integrated[clustered_begin:fixed_begin]
    require(clustered_block,"const bool operator_gate_ready =","clustered operator-local activation gate")
    require(clustered_block,"context != nullptr &&\n            direct_material_ready","clustered context+material prerequisite")
    prepare_call=clustered_block.find("g_clustered_pnts.prepare_sidecar(")
    operator_gate=clustered_block.find("const bool operator_gate_ready =")
    if prepare_call<0 or operator_gate<0 or not operator_gate<prepare_call:
        fail("clustered sidecar is not downstream of the PointLight-local gate")
    for forbidden,label in [
        ("prepare_clustered_pointlight_material_requests","legacy clustered equipment texture prep"),
        ("prepare_fixed_pointlight_material_requests","fixed equipment texture prep"),
        ("prepared.resources","equipment resource batch"),
        ("clustered_resources_not_ready","legacy equipment resource failure gate")
    ]:
        if forbidden in clustered_block:
            fail(f"clustered PointLight still depends on {label}")
    if "prepare_clustered_pointlight_material_requests" in material_resource_cpp or \
       "prepare_clustered_pointlight_material_requests" in material_resource_h:
        fail("legacy clustered material-resource API still exists")

    if "clustered.additional_owners =\n                point | mr | local_if_spc;" in integrated:
        fail("clustered request duplicates primary in additional_owners")
    if "clustered.additional_resource_owners =\n                point | local_if_spc;" in integrated:
        fail("clustered resource ownership duplicates primary")

    require(materializer_h,"composed_shader_owners","materializer composed shader owner ABI")
    require(materializer_h,"clustered_pnts_required_composed_shader_owners","exact clustered static-owner contract")
    require(materializer_cpp,"attest_composed_shader_owners","journal-derived static owner attestation")
    require(materializer_cpp,"migrate_legacy_pointlight_b12_to_current","legacy-to-current clustered b12 ABI migration")
    require(materializer_h,"current_b12_abi","current clustered b12 ABI attestation")
    require(materializer_h,"legacy_specular_complete","clustered Spc legacy-specular attestation")
    require(pipe_cpp,"!outcome.current_b12_abi","pipeline rejects stale clustered b12 ABI")
    require(pipe_cpp,"!outcome.legacy_specular_complete","pipeline rejects incomplete clustered Spc")
    require(integrated,"clustered_spc_legacy_specular_current_b12_ready","runtime positive Spc attestation marker")
    require(integrated,"clustered_spc_failopen_legacy_specular_attestation","runtime Spc anti-hybrid fail-open marker")
    if "clustered_spc_failopen_unmaterialized_legacy_specular" in integrated:
        fail("obsolete unconditional clustered Spc hold-off remains in integrated runtime")
    require(materializer_cpp,"fail_patch_precondition","composition drift fail-open")
    require(pipe_cpp,"composed_shader_owners","candidate/bound shader ownership transport")
    require(pipe_h,"composed_shader_owners","prepared shader ownership ABI")
    require(integrated,"static_shader_owners","draw-time static shader owner transport")
    require(integrated,"expected_static_shader_owners","draw-time owner mask verification")
    require(integrated,"dynamic_additional_owners","narrow dynamic owner mask")
    require(integrated,"dynamic_additional_owners |\n                static_shader_owners","full composed owner set")
    require(integrated,"clustered.additional_shader_owners =\n                clustered.additional_owners;","full replacement shader ownership")
    require(integrated,"clustered.additional_constant_buffer_owners =\n                dynamic_additional_owners;","static operators excluded from b12 ownership")
    require(integrated,"12u,\n                prepared.clustered_carrier.b12","clustered b12 slot")
    require(integrated,"18u,\n                prepared.clustered_carrier.t18","clustered t18 slot")
    require(integrated,"19u,\n                prepared.clustered_carrier.t19","clustered t19 slot")
    require(integrated,"k_route_clustered_pointlight","clustered route bit")
    require(integrated,"dsrrl::core::operator_id::point_light,","full PointLight feature enable")
    require(integrated,"dsrrl::core::operator_id::local_specular_legacy,","legacy spec feature enable")
    require(integrated,"g_clustered_pnts.install()","clustered producer install")
    require(integrated,"const bool clustered_pointlight_init_exact =","clustered init-time exact authority")
    require(integrated,"if (clustered_pointlight_init_exact)","clustered init-time route publication")
    require(integrated,"g_clustered_pnts_pipeline.on_bind_pipeline","clustered pipeline bind")
    global_bind_marker=integrated.find(
        "// PointLight bind work is restricted to pipelines that were attested by"
    )
    if global_bind_marker < 0:
        fail("PointLight bind-time route gate is missing")
    bind_tail=integrated[global_bind_marker:global_bind_marker+2200]
    require(
        bind_tail,
        "(route_mask &\n         k_route_clustered_pointlight) != 0u",
        "clustered bind restricted to init-attested route",
    )
    pre_bind=integrated[
        integrated.find("auto route_mask =", global_bind_marker-4000):
        global_bind_marker
    ]
    if "g_clustered_pnts_pipeline.on_bind_pipeline(" in pre_bind or \
       "g_fixed_pointlight_pipeline.on_bind_pipeline(" in pre_bind:
        fail("PointLight exact registries are still queried on every pixel bind")

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

    # Final draw closure: immediate-context clustered PointLight prefers the
    # native original-draw bridge (one host Draw, no ReShade replay). Deferred
    # or unavailable native paths retain the shared replay transaction as an
    # exact fail-safe. Both paths restore the captured shader/CB/SRV state.
    require(integrated,"prepared.clustered_in_batch","clustered native original-draw eligibility")
    require(integrated,"g_pmetal_native_draw.arm_draw(","native original non-indexed draw bridge")
    require(integrated,"g_pmetal_native_draw.arm_draw_indexed(","native original indexed draw bridge")
    require(native_draw,"core::operator_id::point_light","native bridge PointLight ownership gate")
    require(native_draw,"capture_state(","native original-draw state capture")
    require(native_draw,"apply_mutation(","native original-draw mutation")
    require(native_draw,"restore_state(","native original-draw restore")
    require(adapter,"transactions.replay_draw(","non-indexed replay fallback")
    require(adapter,"transactions.replay_draw_indexed(","indexed replay fallback")
    require(transaction,"PSSetShader(","fallback pixel-shader mutation/restore")
    require(transaction,"restore_ps_constant_buffer_window","fallback constant-buffer restore")
    require(transaction,"PSSetShaderResources(","fallback SRV mutation/restore")
    require(transaction,"return restore(cmd_list, state)","fallback post-draw restore gate")
    require(transaction,"issued_restore_failed","fallback restore failure quarantine path")

    print("Clustered PntS pre-runtime activation source audit: PASS")
    print("  receivers=36 spc=24 nospc=12 stock_membership=t16/t17_bypassed")
    print("  producer=builder-input-snapshot>exact-PointLight-material-prefilter>source-stage(first4+PTDE-cache)>material-stage(sidecar)")
    print("  carrier=b12[3].x+t18+t19 material_max=uint32 min_after_first4")
    print("  material=25 exact HOMOLOGOUS_NOSPC pairs; identity=certified supplement; c100=bit-exact router authority")
    print("  chain=create-time-shader>source-stage(NoSpc+Spc)>material-stage>draw cached payload>native-original-draw or replay-fallback>restore")
    print("  equipment_textures=independent; no clustered Diffuse/Normal/SpecRGB prerequisite")
    print("  zero_light=exact first-four empty membership is neutral stock-equivalent no-op, not fail-open")
    print("  d3d11_context=immediate_or_deferred; dynamic WRITE_DISCARD carrier; stage telemetry enforced")
    print("  shader_owners=dynamic PointLight/MR/local-spec + exact static diffuse-domain/attenuation/SAT (+NoSpc EnvSpec-delete)")
    print("  cb_resources=dynamic owners only; static create-time owners remain shader-only")
    print("  draw=exact pipeline route + cached authority; immediate=native original draw, fallback=shared replay transaction")
    return 0

if __name__=="__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"Clustered PntS pre-runtime activation source audit: FAIL: {exc}",file=sys.stderr)
        raise SystemExit(1)
