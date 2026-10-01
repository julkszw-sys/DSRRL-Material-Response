#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path("renderer-core").resolve()

tex = (root / "src/runtime/texture_identity_transport.cpp").read_text(encoding="utf-8")
flver = (root / "src/runtime/flver_engine_hooks.cpp").read_text(encoding="utf-8")
pmetal_h = (root / "include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp").read_text(encoding="utf-8")
pmetal_cpp = (root / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
producer = (root / "src/runtime/pmetal_producer_state.cpp").read_text(encoding="utf-8")
draw_tx_h = (root / "include/dsrrl/runtime/draw_state_transaction.hpp").read_text(encoding="utf-8")
draw_tx_cpp = (root / "src/runtime/draw_state_transaction.cpp").read_text(encoding="utf-8")
pixel_shadow_h = (root / "include/dsrrl/runtime/pixel_srv_shadow.hpp").read_text(encoding="utf-8")
pixel_shadow_cpp = (root / "src/runtime/pixel_srv_shadow.cpp").read_text(encoding="utf-8")

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
require(pmetal_h, "source_generation", "b12 generation cache")
require(pmetal_cpp, "source.generation", "generation-gated b12 upload")
forbid(pmetal_cpp, "found->second.payload.data()", "legacy 64-byte b12 memcmp cache")
require(pmetal_cpp, "pixel_srv_shadow_snapshot(", "SRV shadow fast path")
require(pmetal_cpp, "prepare_draw_requests_bound(", "bound material resource path")
require(pmetal_cpp, "prepare_bound(", "bound EnvSpec resource path")


require(pixel_shadow_cpp, "descriptor_type::sampler", "sampler shadow tracking")
require(pixel_shadow_cpp, "descriptor_type::constant_buffer", "constant-buffer shadow tracking")
require(pixel_shadow_h, "pixel_cb_shadow_snapshot(", "constant-buffer shadow snapshot API")
require(pixel_shadow_h, "pixel_sampler_shadow_snapshot(", "sampler shadow snapshot API")
require(draw_tx_h, "bool owns_reference = false;", "transaction shadow COM ownership")
require(draw_tx_cpp, "pixel_srv_shadow_snapshot(", "transaction SRV shadow capture")
require(draw_tx_cpp, "pixel_sampler_shadow_snapshot(", "transaction sampler shadow capture")
require(draw_tx_cpp, "pixel_cb_shadow_snapshot(", "transaction constant-buffer shadow capture")
require(draw_tx_cpp, "cb_shadow_capture_", "constant-buffer shadow activation telemetry")
require(draw_tx_cpp, "capture.srv->AddRef();", "transaction SRV shadow lifetime")
require(draw_tx_cpp, "capture.sampler->AddRef();", "transaction sampler shadow lifetime")
require(draw_tx_cpp, "srv_shadow_capture_", "SRV shadow activation telemetry")
require(draw_tx_cpp, "sampler_shadow_capture_", "sampler shadow activation telemetry")

print("DSRRL_RUNTIME_V2_HOTPATH_AUDIT_PASS")
