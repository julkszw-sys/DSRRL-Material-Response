#!/usr/bin/env python3
"""V350 PTDE P_Metal source: exact hit is read-only, miss retains exclusive publish.
SOURCE/CONSTRUCTION contract only. Not a runtime FPS or pixel-equivalence claim.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
source=(root/"renderer-core/src/runtime/pmetal_env_source_runtime.cpp").read_text("utf-8")
builder=(root/"renderer-core/tools/build_v350_qpc_pmetal_factorial.ps1").read_text("utf-8")
def between(a,b):
    assert a in source and b in source, (a,b)
    return source.split(a,1)[1].split(b,1)[0]
lookup=between("bool hook_producer_cache_lookup(","void hook_producer_cache_publish_exact(")
publish=between("void hook_producer_cache_publish_exact(","bool hook_producer_cache_current_exact(")
validator=between("bool hook_producer_cache_current_exact(","std::atomic_bool g_hook_restore_failed")
producer=between("void publish_hook_source(","bool latest_hook_source_exact_selector(")
tls=producer.split("    if (g_hook_source_tls.valid &&",1)[1].split("    hook_source_record cached_record{}",1)[0]
shared=producer.split("    if (hook_producer_cache_lookup(",1)[1].split("    std::uint64_t resolved_version",1)[0]
miss=producer.split("    std::uint64_t resolved_version",1)[1]
assert "std::shared_lock<std::shared_mutex>" in lookup
assert "std::try_to_lock" in lookup
assert "std::unique_lock<std::shared_mutex>" in publish
assert "entry.record = record;" in publish
for x in ["hook_producer_cache_lookup(","cached.semantic_version == record.semantic_version",
          "cached.source.generation == record.source.generation",
          "cached.source.phn_k135_valid ==","cached.source.phn_k135 =="]:
    assert x in validator,x
assert "!hook_producer_cache_current_exact(" in tls
assert "hook_producer_cache_publish_exact(" in tls
assert "if (!g_hook_producer_readonly_hit_enabled.load(" in shared
assert "hook_producer_cache_publish_exact(" in shared
assert "DSRRL_PMETAL_CACHE_MODE.txt" in source
assert "mode=0 exact_shared_hit=EXCLUSIVE_REPUBLISH" in source
assert "mode=1 exact_shared_hit=READ_ONLY" in source
assert "load_hook_readonly_hit_mode();" in source
assert "std::memory_order_release" in source
assert "hook_producer_cache_current_exact(" in tls
assert "hook_producer_cache_publish_exact(" in miss, "unknown source always publishes"
assert "g_hook_source_tls = {" in tls and "g_hook_source_tls = {" in shared
assert "g_hook_source_serial.fetch_add(" in producer
assert "g_hook_source_mutex" in miss, "authoritative fallback retained"
assert "latest_hook_source_exact_selector(" in source
for v in ["DSRRL_V350_QPC_PMETAL_FACTORIAL.addon64",
          "v350_rc1_qpc_pmetal_factorial",
          "DSRRL_FLVER_SELECTOR_PROFILE=OFF",
          "DSRRL_POINTLIGHT_PROFILE=OFF",
          "DSRRL_V350_RELEASE_SKIP_HOT_OBSERVERS"]:
    assert v in builder,v
print("V350 PMETAL EXACT READONLY HIT SOURCE PASS")
print("SAME MATERIAL/SHADER/PTDE SOURCE; NATIVE/PIXEL/FPS TO VERIFY")
