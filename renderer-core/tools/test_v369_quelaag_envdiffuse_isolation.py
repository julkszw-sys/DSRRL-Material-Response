#!/usr/bin/env python3
"""V369 source-level RX33/RX34 EnvDiffuse-only split. No runtime/pixel claim."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
header=(root/"include/dsrrl/operators/env_spec/classic_envdiffuse_rx34_decode_v365.hpp").read_text()
runtime=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
api=(root/"include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp").read_text()
integrated=(root/"integrated/integrated_addon.cpp").read_text()
cmake=(root/"integrated/CMakeLists.txt").read_text()
assert "DSRRL_V369_RX33_ENVDIFFUSE_SPLIT" in cmake
assert "k_rx33_stock_bytes=19856u" in header
assert "k_rx33_candidate_bytes=k_rx33_stock_bytes+48u" in header
assert "0x35,0x88,0x0c,0x0b" in header
assert "const bool rx33=n==k_rx33_stock_bytes && sha==k_rx33_stock_sha256" in header
for needle in ("site_hits!=1u","temp_hits!=1u","fix_checksum","checksum_container_valid",
               "k_dsr_t11_sample","k_dsr_source","k_dsr_merge"):
    assert needle in header,needle
assert "classic_rx33_shader_" in api
assert runtime.count("classic_rx33_shader_->Release()")==2
assert "register_classic_rx33_from_stock(" in runtime
assert "decision.receiver_id != 33u && decision.receiver_id != 34u" in runtime
assert "prepare_envdiffuse_bound(" in runtime
assert "prepare_classic_envdiffuse_bound(" in runtime
source=island=runtime.split("prepare_classic_envdiffuse_only(",1)[1].split("register_lerp_replacement(",1)[0]
assert "env_resources_.prepare_bound(" not in island
assert "constant_buffers[0]" not in island
assert "11u, prepared.envdiffuse_resources.ptde_a" in island
assert "11u, prepared.envdiffuse_resources.sampler" in island
create=integrated.split("bool on_create_pipeline(",1)[1].split(
    "if (!k_drawtime_islands_runtime_enabled)",1)[0]
assert "register_classic_rx33_from_stock(" in create
assert "k_rx33_stock_bytes" in create
assert "pixel_shader->code =" not in create
begin=integrated.split("if (exact_pmetal_envspec) {",1)[1]
priority=begin.split("g_pmetal_envspec.prepare(",1)[0]
assert "DSRRL_V369_RX33_ENVDIFFUSE_SPLIT" in priority
assert "prepare_classic_envdiffuse_only(" in priority
assert "return false;" in priority
assert "receiver_id==33u || decision.receiver_id==34u" in priority
assert "return true;" in priority
assert integrated.count("v369_envdiffuse_isolated_tx(prepared.batch.mutation,dispatch.transaction,")==2
assert "mutation.owners!=owner" in integrated
assert "ptde_t11=1 stock_t12=1 pixel=OPEN" in integrated
assert "V369_RX33_ENVDIFFUSE_SPLIT" in integrated
print("V369 RX33/RX34 SOURCE CONTRACT PASS: independent preferred; failed preflight native fail-open")
print("RX33 RUNTIME REGISTRATION, CUTSCENE GPU COMMIT AND PTDE PIXELS = OPEN")
