#!/usr/bin/env python3
"""342 candidate source aliases with inherited exact resource gates; no GPU claim."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
hdr=(root/"include/dsrrl/operators/env_spec/generated_gi_name_binder_v359.hpp").read_text("utf8")
src=(root/"src/runtime/envspec_resource_runtime.cpp").read_text("utf8")
build=(root/"tools/build_v230_reflection_342_alias.ps1").read_text("utf8")
rows=re.findall(r'\{L"(GI_EnvSpc_m\d{2}_\d{2}_\d{4}_0)",\s*(\d+)u,\s*(\d+)u\}',hdr)
assert len(rows)==342, len(rows)
aliases={}
for name,ordstr,mapstr in rows:
    key="GI_Reflection_"+name[len("GI_EnvSpc_"):-2]
    assert key not in aliases, key
    ordinal=int(ordstr)
    assert 0<=ordinal<342
    assert int(mapstr)==int(name[11:13])
    aliases[key]=ordinal
assert set(aliases.values())==set(range(342))
assert src.count("// V230 GI_Reflection selector5 -> 342 same-packed-ID source-name bridge")==1
for gate in ("DSRRL_V230_REFLECTION_TO_342","g_v359_seen_count[r.source_map]<2u",
             "g_v359_map_conflict[r.source_map]","srv->GetResource(&backing)",
             "v359_verified_native_name_probe_locked(","source_name_exact"):
    assert gate in src, gate
assert "DSRRL_V230_REFLECTION_TO_342" in build
assert "materialize_v230_reflection_342_name_alias.py" in build
print("PASS: 342 unique GI_Reflection name aliases; V358/V359 identity+two-anchor fail-open")
print("OPEN: observed 15 unknown BC6H SHA to original producer name/GPU/pixel")

hook=(root/"src/runtime/texture_identity_transport.cpp").read_text("utf8")
assembly=(root/"integrated/texture_identity_detour.asm").read_text("utf8")
assert "0x2017F4u" in hook
assert "GI REFLECTION SOURCE HOOK" in hook
assert "dsrrl_gi_reflection_branch_entry PROC" in assembly
assert "/DDSRRL_V230_GI_REFLECTION_SOURCE_BRANCH" in build
assert "GI REFLECTION NAME CANDIDATE" in (root/"tools/materialize_v230_release_cleanup.py").read_text("utf8")
