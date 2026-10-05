#!/usr/bin/env python3
from pathlib import Path
import sys
import re

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path("renderer-core").resolve()

tex = (root / "src/runtime/texture_identity_transport.cpp").read_text(encoding="utf-8")
flver = (root / "src/runtime/flver_engine_hooks.cpp").read_text(encoding="utf-8")
pmetal_h = (root / "include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp").read_text(encoding="utf-8")
pmetal_cpp = (root / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
producer = (root / "src/runtime/pmetal_producer_state.cpp").read_text(encoding="utf-8")
pmetal_source = (root / "src/runtime/pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")
clustered = (root / "src/runtime/clustered_pnts_draw_runtime.cpp").read_text(encoding="utf-8")

def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise RuntimeError(f"{label}: missing {needle!r}")

def forbid(text: str, needle: str, label: str) -> None:
    if needle in text:
        raise RuntimeError(f"{label}: forbidden {needle!r}")

def forbid_fetch_add(text: str, counter: str, label: str) -> None:
    if re.search(rf"{re.escape(counter)}\s*\.\s*fetch_add\s*\(", text):
        raise RuntimeError(f"{label}: production hot counter still performs atomic fetch_add: {counter}")

require(tex, "k_logical_name_capacity = 512u", "fixed texture identity buffer")
require(tex, "VirtualQuery(", "page-bounded texture validation")
forbid(tex, "for (std::size_t i = 0;\n             i < k_max;", "legacy per-character texture validation")
require(tex, "snapshot_raw(", "zero-allocation texture snapshot")

parse_body = flver.split("void __fastcall parse_entry", 1)[1].split("void __fastcall destroy_entry", 1)[0]
destroy_body = flver.split("void __fastcall destroy_entry", 1)[1].split("void __fastcall mtd_entry", 1)[0]
forbid(parse_body, "pmetal_env_source_cache_invalidate", "R41 FLVER parse must not flush P_Metal LightBank cache")
forbid(destroy_body, "pmetal_env_source_cache_invalidate", "R41 FLVER destroy must not flush P_Metal LightBank cache")
require(pmetal_source,
        "[DSRRL PMETAL R41] flver_lifecycle_cache_flush=OFF source_cache_generation=SOURCE_RUNTIME_RESET_ONLY endpoint_cache=EXACT_SOURCE_BASE_COUNT_INDEX_ROW bank_cache=BASE_COUNT_LAYOUT region_cache=VM_WINDOW selector_shadow=EXACT_TLS_SOURCE_PTR_SELECTOR_BETA selector_source=ON envspec=ON material_response=ON",
        "R41 P_Metal cache-lifetime runtime attestation")
require(pmetal_source, "latest_hook_source_exact_selector(", "R41 exact native-producer selector shadow join")
require(pmetal_source, "exact_source_ptr=ON exact_selector_beta=ON donor_redecode=OFF", "R41 selector shadow hit attestation")
require(pmetal_source, "k_hook_producer_cache_sets = 256u", "R43 per-key producer cache sets")
require(pmetal_source, "k_hook_producer_cache_ways = 2u", "R43 per-key producer cache associativity")
require(pmetal_source, "capture_hook_selector_identity(", "R43 live source/base/count/row freshness capture")
require(pmetal_source, "hook_producer_cache_lookup(", "R43 cross-thread producer cache lookup")
require(pmetal_source, "std::try_to_lock", "R43 selector cache lookup must not wait")
require(pmetal_source, "[DSRRL PMETAL R43] producer_cache=SET256_WAY2", "R43 startup attestation")
require(pmetal_source, "[DSRRL PMETAL R43] selector_per_key_cache_hit=1", "R43 selector per-key hit attestation")
require(pmetal_source, "[DSRRL PMETAL R43] producer_per_key_cache_hit=1 exact_key=SOURCE_BASE_COUNT_ROW_SELECTOR_BETA exact_payload=ON global_publish_mutex=SKIPPED", "R43 producer per-key hit attestation")

selector = flver.split('extern "C" void dsrrl_flver_selector_observer', 1)[1].split('bool install(', 1)[0]
require(selector, "g_selector_hemdir3_enabled.load", "HemDir3 selector gate")
require(selector, "g_selector_upper_lower_enabled.load", "UpperLower selector gate")
if selector.index("g_selector_hemdir3_enabled.load") > selector.index("hemdir3_mode_transport::selector_begin"):
    raise RuntimeError("HemDir3 selector gate occurs after hot-path call")
if selector.index("g_selector_upper_lower_enabled.load") > selector.index("upper_lower_selector_event_bridge"):
    raise RuntimeError("UpperLower selector gate occurs after hot-path call")

require(producer, "g_record.generation + 1u", "semantic generation increment")
require(producer, "same_source_payload", "semantic payload equality")
require(pmetal_source, "same_hook_source_payload(", "fallback semantic payload equality")
require(pmetal_source, "g_hook_source_generation", "fallback semantic generation")
require(pmetal_source, "g_hook_source_semantic_version", "legacy latest-source fallback semantic version")
lookup_start=pmetal_source.find("bool hook_producer_cache_lookup(")
lookup_end=pmetal_source.find("void hook_producer_cache_publish_exact(",lookup_start)
if min(lookup_start,lookup_end)<0:
    fail("R43 producer-cache lookup boundary missing")
lookup_body=pmetal_source[lookup_start:lookup_end]
forbid(lookup_body, "g_hook_source_semantic_version", "R43 per-key cache must not use process-global semantic-version freshness")
require(pmetal_source, "same_hook_selector_identity(", "R43 exact per-key live identity gate")
latest_start=pmetal_source.find("bool pmetal_env_source_runtime::latest(")
latest_end=pmetal_source.find("pmetal_env_source_runtime_telemetry",latest_start)
if min(latest_start,latest_end)<0:
    fail("R44 visible latest() boundary missing")
latest_body=pmetal_source[latest_start:latest_end]
forbid(latest_body, "latest_hook_source(out)", "R44 visible EnvSpec path must not consume unkeyed latest hook source")
require(pmetal_source,
        "[DSRRL PMETAL R44] visible_source_authority=MATERIAL_BOUND_PRODUCER_STATE_ONLY unkeyed_latest_hook_fallback=OFF exact_selector_shadow=ON missing_join=FAIL_OPEN_STOCK_DSR islands_preserved=ON",
        "R44 material-bound source authority attestation")

require(pmetal_source,
        "[DSRRL PMETAL R45] production_hot_counters=GATED redundant_cross_thread_republish=OFF decoded_endpoint_identity_reuse=ON renderer_semantics=UNCHANGED",
        "R45 production hot-path cleanup attestation")

for counter in (
    "g_hook_producer_cache_busy",
    "g_hook_producer_cache_hit",
    "g_hook_producer_cache_miss",
    "g_hook_producer_cache_publish",
    "g_region_cache_hit",
    "g_region_cache_miss",
    "g_endpoint_cache_hit",
    "g_endpoint_cache_miss",
    "g_endpoint_cache_fill",
    "g_bank_signature_scan_count",
    "g_hook_single_seen",
    "g_hook_blend_seen",
    "g_hook_publish",
    "g_hook_consume",
):
    forbid_fetch_add(
        pmetal_source,
        counter,
        "R45 behavior-independent telemetry must be gated")

decode_start = pmetal_source.find("void record_hook_decode(")
decode_end = pmetal_source.find("struct readable_window", decode_start)
if min(decode_start, decode_end) < 0:
    raise RuntimeError("R45 hook decode diagnostic boundary missing")
require(
    pmetal_source[decode_start:decode_end],
    "if (!telemetry::effect_enabled())",
    "R45 hook decode stores must be effect-telemetry gated")

bank_stage_start = pmetal_source.find("void record_bank_signature_stage(")
bank_stage_end = pmetal_source.find("bool same_hook_source_payload(", bank_stage_start)
if min(bank_stage_start, bank_stage_end) < 0:
    raise RuntimeError("R45 bank signature diagnostic boundary missing")
require(
    pmetal_source[bank_stage_start:bank_stage_end],
    "if (!telemetry::effect_enabled())",
    "R45 bank signature stores must be effect-telemetry gated")

producer_hit_marker = "if (hook_producer_cache_lookup(\n            selector_identity,\n            cached_record,\n            &next))"
producer_hit_start = pmetal_source.find(producer_hit_marker)
producer_hit_end = pmetal_source.find("// R44 removed the unkeyed visible consumer", producer_hit_start)
if min(producer_hit_start, producer_hit_end) < 0:
    raise RuntimeError("R45 producer exact-hit branch missing")
forbid(
    pmetal_source[producer_hit_start:producer_hit_end],
    "hook_producer_cache_publish_exact(",
    "R45 exact cross-thread hit must not republish identical cache entry")

require(
    pmetal_source,
    "std::atomic<std::uint64_t> residency_version{0u};",
    "R45 producer cache slot residency version")
require(
    pmetal_source,
    "hook_producer_cache_residency_live(",
    "R45 TLS exact producer residency fast path")

publish_start = pmetal_source.find("void hook_producer_cache_publish_exact(")
publish_end = pmetal_source.find("std::atomic_bool g_hook_restore_failed", publish_start)
if min(publish_start, publish_end) < 0:
    raise RuntimeError("R45 producer publish boundary missing")
publish_body = pmetal_source[publish_start:publish_end]
residency_gate = publish_body.find("hook_producer_cache_residency_live(")
publish_lock = publish_body.find("std::lock_guard<std::mutex> lock(")
if min(residency_gate, publish_lock) < 0 or residency_gate > publish_lock:
    raise RuntimeError("R45 residency fast path must occur before producer cache mutex")

for latch in (
    "producer_cross_thread_hit_logged",
    "selector_cross_thread_hit_logged",
    "selector_shadow_hit_logged",
):
    require(
        pmetal_source,
        f"!{latch}.load(",
        f"R45 hot one-shot latch {latch} must load-gate atomic exchange")

require(
    pmetal_source,
    "struct hook_endpoint_identity_v1",
    "R45 decoded hook endpoint identity carrier")
require(
    pmetal_source,
    "&decoded_endpoint",
    "R45 single hook endpoint identity reuse")
require(
    pmetal_source,
    "&decoded_a",
    "R45 blend hook endpoint identity reuse")
require(
    pmetal_source,
    "Canonical zero-beta identity must match",
    "R45 zero-beta selector identity canonicalization")
for latch in (
    "cache_hit_logged",
    "selection_cache_hit_logged",
):
    require(
        clustered,
        f"!{latch}.load(",
        f"R45 PointLight hot one-shot latch {latch} must load-gate atomic exchange")

require(
    clustered,
    "pointlight_bank_authority_cache_current()",
    "R45 PointLight semantic-generation donor authority cache")
require(
    clustered,
    "pointlight_decode_access_current()",
    "R45 PointLight per-present VM validation cache")
require(
    clustered,
    "capture_ptde_source_from_frame_state(",
    "R45 PointLight exact frame-state source decode")
forbid(
    clustered,
    "pointlight_ptde_source::capture(",
    "R45 clustered source capture must not redo node/vtable/manager decode")
require(
    clustered,
    "[DSRRL POINTLIGHT R45] bank_authority_cache=TLS_SEMANTIC_GENERATION vm_validation_cache=TLS_PER_PRESENT exact_frame_state_reuse=ON duplicate_vtable_owner_selector_manager_decode=OFF spc=ON nospc=ON",
    "R45 PointLight decode-cache attestation")

forbid(pmetal_source, "next.generation = serial", "event serial must not drive semantic generation")
require(pmetal_h, "payload_bits", "exact b12 payload cache")
require(pmetal_cpp, "payload_bits.data()", "exact 64-byte b12 payload identity")
require(pmetal_cpp, "found->second.payload_bits !=", "payload-gated b12 upload")
forbid(pmetal_h, "source_generation", "generation must not key b12 cache")
forbid(pmetal_cpp, "found->second.source_generation", "event/global generation must not invalidate b12")
require(pmetal_cpp, "pixel_srv_shadow_snapshot(", "SRV shadow fast path")
require(pmetal_cpp, "prepare_draw_requests_bound(", "bound material resource path")
require(pmetal_cpp, "prepare_bound(", "bound EnvSpec resource path")

print("DSRRL_RUNTIME_V2_HOTPATH_AUDIT_PASS")
