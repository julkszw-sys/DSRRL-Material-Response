#!/usr/bin/env python3
"""V301 strict original-source donor routing contract, not runtime/pixel PASS."""
from pathlib import Path
import csv
r=Path(__file__).resolve().parents[1]
src=(r/"src/runtime/pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")
draw=(r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
integ=(r/"integrated/integrated_addon.cpp").read_text(encoding="utf-8")
hdr=(r/"include/dsrrl/runtime/v300_dsr_only_lightbank_donors.hpp").read_text(encoding="utf-8")
t=r/"data/provenance/v300_lightbank_donors_ptde_dsr_only.tsv"
with t.open(encoding="utf-8",newline="") as f:
    rows=list(csv.DictReader(f,delimiter="\t"))
assert len(rows)==1249
assert len([x for x in rows if x["origin"]=="PTDE"])==1246
extra=[x for x in rows if x["origin"]=="DSR_ONLY"]
assert len(extra)==3
for x in extra:
    assert int(x["row_id"])==64
    assert int(x["envdiffuse_m"]) in (150,500,1500)
    assert int(x["envspec_m"]) in (150,200)
    assert x["envdiffuse_ptde_reference"].startswith("DSR_VANILLA_DRAWPARAM_SHA256_")
for needle in (
    "exact_envdiffuse_endpoint(",    # PTDE source: original bin
    "read_exact_source(",          # PTDE EnvSpec same row authority
    "v300::classify_dsr_only(",    # DSR-only exact source class
    "v300::donor_provenance::dsr_only",
    "action=stock_dsr_operator",
    "v299::authored_slot0_source(material)",
    "if (latest_hook_source(out))",
):
    assert needle in src,needle
a=src.index("#if defined(DSRRL_V300_DSR_ONLY_EXACT_DONORS)",src.index("bool pmetal_env_source_runtime::latest("))
stop=src.index("return false;",a)
b=src.index("if (latest_hook_source(out))",a)
assert a<stop<b, "V301 may not reuse an unkeyed PTDE source after exact-key failure"
assert "Do NOT reuse any stale global/TLS PTDE donor" in src
assert "source_.latest(material, source)" in draw
assert "v299::authored_slot0_draw(material, decision)" in draw
assert "g_pmetal_envspec.prepare(" in integ
assert "!v296_partial_material" in integ
assert "1500u" in hdr and "500u" in hdr and "150u" in hdr
assert "DSRRL_V300_DSR_ONLY_EXACT_DONORS" in src
print("V301_MIXED_DONOR_CONTRACT_PASS 1246 PTDE + 3 DSR_ONLY")
print("V301_RUNTIME_AND_PIXEL_OPEN")
