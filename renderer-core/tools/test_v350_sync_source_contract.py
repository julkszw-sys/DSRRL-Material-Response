#!/usr/bin/env python3
"""V350-RC1 source-only thread synchronization and diagnostic traffic invariants.

This does not assert that QPC changed scheduling, or claim runtime FPS.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
src = (root / "renderer-core/src/runtime/pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")
builder = (root / "renderer-core/tools/build_v350_sync_one_addon.ps1").read_text(encoding="utf-8")


def demand(ok: bool, msg: str) -> None:
    if not ok:
        raise SystemExit("V350 SYNC CONTRACT FAIL: " + msg)


def body_between(begin: str, end: str) -> str:
    demand(begin in src and end in src, "missing code region " + begin)
    return src.split(begin, 1)[1].split(end, 1)[0]


demand("#include <shared_mutex>" in src, "C++17 shared mutex include")
demand("std::array<std::shared_mutex,k_hook_producer_cache_sets>" in src,
       "all PMetal producer cache shards use shared mutex")
lookup = body_between("bool hook_producer_cache_lookup(", "void hook_producer_cache_publish_exact(")
publish = body_between("void hook_producer_cache_publish_exact(", "std::atomic_bool g_hook_restore_failed")
demand("std::shared_lock<std::shared_mutex> lock(" in lookup,
       "readers must share a shard rather than exclude other readers")
demand("g_hook_producer_cache_mutex[set]" in lookup and "std::try_to_lock" in lookup,
       "readers preserve nonblocking fail-open behavior")
demand("std::unique_lock<std::shared_mutex> lock(" in publish,
       "writes must remain exclusive")
demand("g_hook_producer_cache_mutex[set]" in publish,
       "writes and readers must use the identical shard")
for exact in ("same_hook_selector_identity(", "same_hook_source_payload("):
    demand(exact in lookup, "source/payload authority preserved: " + exact)
demand("entry.record = record;" in publish and "entry.key = key;" in publish,
       "atomic cache entry publication preserved")

counts = {
    "g_hook_single_seen": 1,
    "g_hook_blend_seen": 1,
    "g_hook_publish": 3,
    "g_hook_consume": 4,
    "g_bank_signature_scan_count": 1,
}
for counter, expected in counts.items():
    demand(src.count("telemetry::hot_count(" + counter + ");") == expected,
           "diagnostic counter properly guarded " + counter)
    demand(counter + ".fetch_add(" not in src,
           "unconditional diagnostic coherence write " + counter)

decode = body_between("void record_hook_decode(", "struct readable_window")
bank = body_between("void record_bank_signature_stage(", "bool same_hook_source_payload(")
for block, name in ((decode, "decode stage"), (bank, "bank signature stage")):
    demand("if (!telemetry::hot_enabled()) return;" in block,
           "hot diagnostic-only atomic fields should be inactive OFF: " + name)

protected = body_between("bool latest_hook_source(", "void clear_hook_source(")
demand("g_hook_source_tls.valid" in protected and
       "g_hook_source_mutex" in protected and
       "g_hook_source_global.source" in protected,
       "protected TLS/global producer fallback preserved")
demand("g_hook_source_serial.fetch_add(" in src,
       "unique producer-event serial is not a diagnostic counter")
demand("same_hook_source_payload(" in src and
       "latest_hook_source_exact_selector(" in src,
       "exact PMetal source/selector consumer preserved")

for expected in (
    "build-v350-sync",
    "out-v350-sync",
    "v350_rc1_sync_shard_read_noqpc",
    "DSRRL_V350_RC1_SYNC_OPTIMIZED.addon64",
    "DSRRL_V350_PTDE_LERP_RGB_COMPONENT_SWIZZLE",
    "DSRRL_V293_PARSER_EXACT_FLVER_ALIAS=ON",
    "DSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF",
    "DSRRL_FLVER_SELECTOR_PROFILE=OFF",
    "DSRRL_POINTLIGHT_PROFILE=OFF",
):
    demand(expected in builder, "build feature/path drift: " + expected)
for forbidden in ("SetThreadAffinityMask", "SetThreadPriority", "Sleep(",
                  "SwitchToThread(", "std::this_thread::yield("):
    demand(forbidden not in lookup + publish,
           "do not mimic QPC via scheduling tricks: " + forbidden)
print("V350 SYNC SOURCE PASS: concurrent read locks/exclusive publishers, nonblocking fail-open, protected source, diagnostic atomics OFF; QPC/FPS causal status OPEN")
