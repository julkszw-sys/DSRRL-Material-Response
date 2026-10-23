#!/usr/bin/env python3
"""V303 donor census plus owner-protected R43 fallback; row64 draw routing OPEN."""
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
    "live_stock_routing=UNVERIFIED",
    "v299::authored_slot0_source(material)",
    "if (latest_hook_source(out))",
):
    assert needle in src,needle
# Owner PROTECT rev13040: preserve the R43 producer fallback on exact-key miss.
# DSR-only static classification is not proof of a current draw/receiver route.
latest=src.split("bool pmetal_env_source_runtime::latest(",1)[1].split("pmetal_env_source_runtime_telemetry",1)[0]
assert latest.count("if (latest_hook_source(out))")==1
assert "DSRRL_V300_DSR_ONLY_EXACT_DONORS" not in latest
between=latest.split("if (pmetal_producer_state_latest(",1)[1].split("if (latest_hook_source(out))",1)[0]
assert "return false;" not in between, "R43 producer fallback disabled before the protected call"
assert "live_stock_routing=UNVERIFIED" in src
assert "action=stock_dsr_operator" not in src
assert "source_.latest(material, source)" in draw
assert "v299::authored_slot0_draw(material, decision)" in draw
assert "g_pmetal_envspec.prepare(" in integ
assert "!v296_partial_material" in integ
assert "1500u" in hdr and "500u" in hdr and "150u" in hdr
assert "DSRRL_V300_DSR_ONLY_EXACT_DONORS" in src
print("V303_PROTECTED_R43_SOURCE_CENSUS_PASS 1246 PTDE + 3 DSR_ONLY; DRAW_ROUTING_OPEN")
print("V301_RUNTIME_AND_PIXEL_OPEN")
