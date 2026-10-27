#!/usr/bin/env python3
"""V408 source contract: bounded diagnostics, no authority, R43 protected."""
from pathlib import Path

base=Path(__file__).resolve().parents[1]
src=(base/"src/runtime/pmetal_env_source_runtime.cpp").read_text()
build=(base/"tools/build_v230_production.ps1").read_text()
p=src[src.index("void pmetal_env_source_selector_event("):src.index("bool pmetal_env_source_runtime::latest(")]
boundary=p.index("    if (!lookup_valid ||\n        source_a == nullptr ||\n        source_b == nullptr)")
diag=p.index("#if defined(DSRRL_V408_CONTEXT_IDENTITY_DIAG)")
next_begin=p.index("#if defined(DSRRL_V398_ACTOR_TRANSACTIONAL_SOURCE)", diag)
assert boundary < diag < next_begin
block=p[diag:next_begin]
assert "n <= 64u || (n % 8192u) == 0u" in block
assert "safe_read(owner, owner_vtable)" in block
assert "g_source_base + 0x12E6B60u" in block
assert "safe_read(owner_bytes + 0x2320u, context_manager)" in block
assert "safe_read(owner_bytes + 0x2338u," in block
assert "manager_matches=%u" in block
assert "fallback_type5_id=%u" in block
assert "vtable_readable && owner_vtable == expected" in block
assert "receiver_join=OPEN pixel=OPEN" in block
assert "pmetal_producer_state_begin" not in block
assert "pmetal_producer_state_publish" not in block
assert "return;" not in block
assert "source_a =" not in block and "source_b =" not in block
assert "latest_hook_source(out)" in src
assert "/DDSRRL_V408_CONTEXT_IDENTITY_DIAG" in build
assert "'[DSRRL V408 CONTEXT SELECTOR CENSUS]'" in build
print("V408 STATIC SOURCE CONTRACT PASS: owner identity diagnostic-only, Source2 protected")

shadow=p[p.index("if (latest_hook_source_exact_selector("):p.index("#if defined(DSRRL_V396_STRICT_NATIVE_SOURCE1)")]
assert "[DSRRL V408 SOURCE1 PACKER JOIN]" in shadow
assert "pmetal_producer_state_latest(" in shadow
assert "committed.native_producer_sequence ==" in shadow
assert "native_producer_seq=%llu selector_epoch=%llu" in shadow
assert "gpu_join=OPEN pixel=OPEN" in shadow
