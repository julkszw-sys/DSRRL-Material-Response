#!/usr/bin/env python3
"""V407 source-level reproduction of the unresolved FrpgDrawContext lifetime frontier.
This test verifies *absence* of an instance generation certificate, not a runtime fix.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
producer=(root/"src/runtime/pmetal_producer_state.cpp").read_text()
source=(root/"src/runtime/pmetal_env_source_runtime.cpp").read_text()
selector=(root/"src/runtime/flver_engine_hooks.cpp").read_text()
s=producer[producer.index("bool same_model("):producer.index("bool same_source_payload(")]
assert "model_owner_instance == b.model_owner_instance" in s
assert "lifetime_generation" not in s and "actor_generation" not in s
key=producer[producer.index("std::uint64_t model_key("):producer.index("std::uint64_t source_key(")]
assert "material.model_owner_instance" in key
assert "return static_cast<std::uint64_t>(material.model_owner_instance);" in key
clear=producer[producer.index("void pmetal_producer_state_clear()"):producer.index("void pmetal_producer_state_begin(")]
assert "g_pending = {};" in clear and "g_record.valid = false;" in clear
assert "g_sync_slots" not in clear
assert "constexpr std::size_t k_sync_slot_count = 4096u" in producer
assert "pmetal_producer_state_actor_tracked(" in producer
assert "model.model_owner_instance =" in selector
assert "reinterpret_cast<std::uintptr_t>(owner)" in selector
assert "latest_hook_source(out)" in source  # Protected R43 fallback preserved.
print("V407 SOURCE CONTRACT PASS: pointer-based cached instance; native per-instance lifetime join OPEN")
