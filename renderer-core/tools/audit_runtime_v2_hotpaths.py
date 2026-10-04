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
require(pmetal_source, "g_hook_source_semantic_version", "fallback semantic version")
require(pmetal_source, "g_hook_source_tls.semantic_version ==", "fallback unchanged-payload lock-free gate")
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
