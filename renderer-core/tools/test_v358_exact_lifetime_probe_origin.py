#!/usr/bin/env python3
"""V358 source gates: exact BC6H create-to-actual-reject typed identity.
Unknown SHA remains fail-open. No PTDE probe guessing, no FLVER owner override.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
env=(root/"src/runtime/envspec_resource_runtime.cpp").read_text("utf8")
draw=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf8")
hdr=(root/"include/dsrrl/runtime/envspec_resource_runtime.hpp").read_text("utf8")
build=(root/"tools/build_v358_exact_lifetime_probe_origin.ps1").read_text("utf8")
for value in (
    "g_v358_lifetime_probes",
    "g_v358_lifetime_next.fetch_add(",
    "g_v358_lifetime_probes.erase(resource_handle.handle)",
    "g_v358_lifetime_probes.clear()",
    "g_v358_lifetime_probes[resource_handle.handle]=item",
    "item.sha=digest",
    "item.certified=probe.has_value()",
    "v358_probe_origin_for_native_view(",
    "view->GetResource(&resource)",
    "view->GetDevice(&native_device)",
    "reinterpret_cast<std::uintptr_t>(native_device)",
    "out.creation_sha256=origin.sha",
):assert value in env,value
for value in (
    "[DSRRL V358 PROBE ORIGIN]",
    "v358_log_probe_reject_native_origin(",
    "native_view[0]",
    "native_view[2]",
    "[DSRRL V354 PROBE REJECT]",
    "action=STOCK_DSR_IF_UNAUTHORIZED pixel=OPEN",
    "v357_trace_rejected_srv(",
): assert value in draw,value
assert draw.count("v358_log_probe_reject_native_origin(")==3
assert draw.index("[DSRRL V354 PROBE REJECT]")<draw.rindex("v358_log_probe_reject_native_origin(")
assert "std::array<std::uint8_t, 32> creation_sha256{};" in hdr
assert "DSRRL_V358_EXACT_PROBE_ORIGIN" in hdr
assert "/DDSRRL_V358_EXACT_PROBE_ORIGIN" in build
assert "materialize_v358_exact_lifetime_probe_origin.py" in build
assert "DSRRL_V358_EXACT_LIFETIME_PROBE_ORIGIN.addon64" in build
assert (root/"data/provenance/user_envspec_intensity_overrides_v355.tsv").read_text("utf8").count("USER_TEST_OVERRIDE")==3
# V358 must never change exact source admission or material owner fail-open.
assert "env::legacy_native_probe_for_sha(" in env
assert "if (!probe.has_value()) {" in env
assert "if (!env_resource_ready) {" in draw
assert "NO_PRODUCER_WITNESS" in draw
print("V358_TYPED_LIFETIME_BC6H_TO_ACTUAL_PROBE_REJECT_CONTRACT_PASS")
