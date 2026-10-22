#!/usr/bin/env python3
"""PR487 exact A14 request-ready diagnostic contract; no render mutation."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=r/'src/runtime/pmetal_envspec_draw_runtime.cpp'
s=p.read_text('utf8')
assert s.count('[DSRRL A14 REQUEST READY]')==1
assert s.count('effect_latch(effect_request_ready_);')==1
i=s.index('[DSRRL A14 REQUEST READY]')
ready=s.index('effect_latch(effect_request_ready_);')
assert ready<i
assert s.index('build_island_draw_mutation(')<i
for marker in ('0xc9b879a8c5e624d0ull','0xe447e0e74af8d1d3ull','static std::atomic<std::uint64_t> ready_seen[24]{}','downstream_draw_commit=NOT_PROVEN','prepared.envdiffuse_resources.ptde_a!=nullptr','prepared.env_resources.ptde_a!=nullptr','source.bank_signature_b','material.semantic_name_hash','owner_exact=%u'):
 assert marker in s,marker
assert s.count('source.envdiffuse_linear_valid?1u:0u')>=1
assert s.count('reshade::log::level::warning,line')>=1
print('A14_REQUEST_READY_POST_GUARD_WITNESS_CONTRACT_PASS: source-only, capped rows, no auto admission')
