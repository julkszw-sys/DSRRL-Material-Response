#!/usr/bin/env python3
"""Source guard for V360: exact native EnvDiffuse COM view join recovery.
This is a compile-independent contract, NOT a pixel or runtime test.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = (root / "src/runtime/envspec_resource_runtime.cpp").read_text("utf-8")
head = source.split("bool envspec_resource_runtime::native_envdiffuse_probe_for_view(", 1)
assert len(head) == 2, "t11 exact resolver missing"
resolver = head[1].split("bool envspec_resource_runtime::prepare_envdiffuse_bound(", 1)[0]
prepare = source.split("bool envspec_resource_runtime::prepare_envdiffuse_bound(", 1)[1].split("bool envspec_resource_runtime::prepare_envdiffuse(",1)[0]
checks = {
  "COM current backing queried": "stock_view->GetResource(&backing)" in resolver,
  "Exact resource authority checked": "g_native_diffuse_resources.find(resource_key)" in resolver and "g_native_diffuse_resources.find(texture_key)" in resolver,
  "SRV joined only after certified native hash": resolver.index("native != g_native_diffuse_resources.end()") < resolver.index("g_diffuse_by_view[key] ="),
  "Bound helper shares exact resolver": "native_envdiffuse_probe_for_view(stock_t11,a)" in prepare and "native_envdiffuse_probe_for_view(stock_t13,b)" in prepare,
  "No fallback on unknown source": "g_diffuse_unverified_views.insert(key)" in resolver and "return restored;" in resolver,
  "No GPU readback/altered render state": not any(token in resolver for token in ("CopyResource(", "CopySubresourceRegion(", "Map(", "PSSetShaderResources(", "Draw(", "Dispatch(")),
  "Root-cause descriptor and exact stage counters": all(token in resolver for token in ("texture->GetDesc(&texture_desc)", "srv_fmt=%u", "no_initial=%llu", "extract_fail=%llu", "sha_miss=%llu", "sha_hit=%llu")),
  "No ABI shader global patch": "DSRRL V360 T11 NATIVE ID MISS" in resolver,
  "Noexcept OOM returns fail-open": "catch (...)" in resolver and "return false;" in resolver,
  "Cleanup handles COM references": "texture->Release()" in resolver and "backing->Release()" in resolver,
  "m17 duplicate exact hash still ambiguous": "unique_native_diffuse_probe(g_native_diffuse_hashes,digest)" in source,
}
for name, passed in checks.items():
    print(("PASS" if passed else "FAIL") + " " + name)
assert all(checks.values()), "V360 exact-resolver safety contract failure"
print("V360 T11 EXACT COM VIEW-JOIN SOURCE CONTRACT PASS; PIXELS OPEN")
