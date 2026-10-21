#!/usr/bin/env python3
"""V361 create SHA -> exact native SRV join witness. No selector relaxation."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/"src/runtime/envspec_resource_runtime.cpp").read_text("utf-8")
v358=(root/"tools/materialize_v358_exact_lifetime_probe_origin.py").read_text("utf-8")
a=s.index("void on_init_resource(")
b=s.index("void on_destroy_resource(",a)
creation=s[a:b]
a=s.index("bool envspec_resource_runtime::native_envdiffuse_probe_for_view(")
b=s.index("bool envspec_resource_runtime::prepare_envdiffuse_bound(",a)
join=s[a:b]
checks={
 "read-only capture from creation-tight BC6H": "const auto digest = hashing::sha256(tight.data(),tight.size());" in creation and
   "g_diffuse_creation_sha[resource_handle.handle] = digest;" in creation,
 "original 342 atlas guard remains": "env::unique_native_diffuse_probe(g_native_diffuse_hashes,digest)" in creation,
 "missing resource remains rejected": "if (!probe.has_value())" in creation and "return;" in creation,
 "native COM exact backing recovered": "stock_view->GetResource(&backing)" in join and
   "g_diffuse_creation_sha.find(resource_key)" in join,
 "creation witness independent from authorization": join.index("g_diffuse_creation_sha.find(resource_key)") <
   join.index("g_native_diffuse_resources.find(resource_key)"),
 "retail source identity is same native resource": "v214_resource_provenance::instance().snapshot(" in join and
   "reinterpret_cast<std::uintptr_t>(backing)" in join,
 "explicit uncertainty emitted": all(k in join for k in ("NO_PRODUCER_WITNESS","AMBIGUOUS_SOURCE","UNAVAILABLE","create_sha_join=%u","atlas=UNMATCHED action=STOCK_DSR")),
 "source file doesn't rewrite render state": not any(k in join for k in ("PSSetShaderResources(","CopyResource(","Draw(","Dispatch(")),
 "generation maintains native digest on destroy": v358.count("g_diffuse_creation_sha.erase(resource_handle.handle);")==2,
 "lifetime cleaned": "g_diffuse_creation_sha.clear();" in s and "g_diffuse_creation_sha.erase(resource_handle.handle);" in s,
 "one addon existing authority intact": "DSRRL_V360_T11" not in s,
}
for name,ok in checks.items():print(("PASS" if ok else "FAIL"),name)
assert all(checks.values()),"V361 exact creation provenance contract failed"
print("V361 exact SHA witness SOURCE PASS; NO PIXEL/BRIDGE ACTIVATION CLAIM")
