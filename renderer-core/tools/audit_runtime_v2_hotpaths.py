#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path("renderer-core").resolve()

tex = (root / "src/runtime/texture_identity_transport.cpp").read_text(encoding="utf-8")
flver = (root / "src/runtime/flver_engine_hooks.cpp").read_text(encoding="utf-8")
pmetal_h = (root / "include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp").read_text(encoding="utf-8")
pmetal_cpp = (root / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
producer = (root / "src/runtime/pmetal_producer_state.cpp").read_text(encoding="utf-8")
pmetal_source = (root / "src/runtime/pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")

def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise RuntimeError(f"{label}: missing {needle!r}")

def forbid(text: str, needle: str, label: str) -> None:
    if needle in text:
        raise RuntimeError(f"{label}: forbidden {needle!r}")

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
exact_selector_start = pmetal_source.find("bool latest_hook_source_exact_selector(")
exact_selector_end = pmetal_source.find("bool latest_hook_source(", exact_selector_start)
if min(exact_selector_start, exact_selector_end) < 0:
    raise RuntimeError("R43 exact-selector producer-cache boundary missing")
exact_selector_body = pmetal_source[exact_selector_start:exact_selector_end]
require(exact_selector_body, "same_hook_selector_identity(", "R43 exact selector TLS identity gate")
require(exact_selector_body, "hook_producer_cache_lookup(", "R43 exact selector cross-thread producer cache join")
forbid(exact_selector_body, "g_hook_source_semantic_version", "R43 exact selector join must not use process-global semantic freshness")

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
