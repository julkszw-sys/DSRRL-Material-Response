#!/usr/bin/env python3
"""V304 static source contract: provenance entry is an active typed source.

This cannot prove which selector/receiver route occurs in game, or pixels.
"""
from pathlib import Path
import csv
r=Path(__file__).resolve().parents[1]
src=(r/"src/runtime/pmetal_env_source_runtime.cpp").read_text("utf-8")
draw=(r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
hdr=(r/"include/dsrrl/runtime/v300_dsr_only_lightbank_donors.hpp").read_text("utf-8")
p=r/"data/provenance/v300_lightbank_donors_ptde_dsr_only.tsv"
with p.open(newline="",encoding="utf-8") as f:
    records=list(csv.DictReader(f,delimiter="\t"))
assert len(records)==1249
assert sum(row["origin"]=="PTDE" for row in records)==1246
native=[row for row in records if row["origin"]=="DSR_ONLY"]
assert len(native)==3
assert {row["area"] for row in native}=={"a14","a18"}
assert {row["bank_name"] for row in native}=={"m14_LightBank.param","s14_LightBank.param","m18_LightBank.param"}
assert all(int(row["row_id"])==64 for row in native)
assert all(int(row["envspec_r"])==255 and int(row["envdiffuse_r"])==255 for row in native)
for s in ("v300::classify_dsr_only(","typed.dsr_values->dsr_envspec",
          "v300::decode_linear(dsr.dsr_envdiffuse)","endpoint_cache_publish(source,base,count,index,row_id,out,signature);"):
    assert s in src,s
assert "DSRRL_V304_TYPED_DSR_ONLY_DONOR" in src
assert "DSRRL_V304_TYPED_DSR_ONLY_DONOR" in draw
assert "v304_typed_dsr_only_native_draw" in draw
assert "source.row_id_a == dsr.row_id" in draw
assert "source.row_id_b == dsr.row_id" in draw
assert "dsr_only_draw=true;" in draw
consumer=draw.index("v304_typed_dsr_only_native_draw")
gpu=draw.index("auto *context =",consumer)
assert consumer<gpu, "cannot modify D3D11 state before donor stock guard"
assert draw.index("source_.latest(material, source)")<consumer
latest=src.split("bool pmetal_env_source_runtime::latest(",1)[1].split("pmetal_env_source_runtime_telemetry",1)[0]
assert latest.count("if (latest_hook_source(out))")==1
assert "DSRRL_V304_TYPED_DSR_ONLY_DONOR" not in latest
assert "DSRRL_V300_DSR_ONLY_EXACT_DONORS" not in latest
assert "return false;" not in latest.split("if (pmetal_producer_state_latest(",1)[1].split("if (latest_hook_source(out))",1)[0]
assert "inline constexpr std::array<dsr_lightbank_row,3> k_dsr_only" in hdr
print("V304_TYPED_SOURCE_CONSTRUCTION_PASS rows=1249 PTDE=1246 DSR_ONLY=3 owner_protect=RETAINED")
print("V304_GPU_RUNTIME_AND_PTDE_PIXELS_OPEN")
