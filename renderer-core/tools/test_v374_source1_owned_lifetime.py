#!/usr/bin/env python3
"""V374 Source1 scope contract. This does not assert GPU/pixel equivalence."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
src = (root / "src/runtime/pmetal_env_source_runtime.cpp").read_text("utf-8")
state = (root / "src/runtime/pmetal_producer_state.cpp").read_text("utf-8")
start = src.index("void pmetal_env_source_selector_event(")
end = src.index("bool pmetal_env_source_runtime::latest(", start)
selector = src[start:end]
parent = selector.index("parent_return !=")
descriptor = selector.index("if (descriptor_owner != owner ||")
begin = selector.index("pmetal_producer_state_begin(")
publish = selector.index("pmetal_producer_state_publish(")
assert parent < descriptor < begin < publish
assert selector.count("pmetal_producer_state_begin(") == 1
assert "pmetal_producer_state_clear();" in selector[:parent]
assert "pmetal_producer_state_begin(" in selector[descriptor:]
assert "latest_hook_source_exact_selector(" in selector
assert "bool latest_hook_source(" in src
consumer = src[src.index("bool pmetal_env_source_runtime::latest("):]
assert consumer.index("pmetal_producer_state_latest(") < consumer.index("latest_hook_source(out)")
assert "g_hook_source_tls.valid" in src
assert "g_hook_source_global.valid" in src
assert "slot.valid.store(" in state
assert "same_material(" in state
assert "slot.record.epoch != epoch" in state
draw = (root / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
integrated = (root / "integrated/integrated_addon.cpp").read_text("utf-8")
assert "out.consumer_source_kind = 1u;" in consumer
assert "out.consumer_source_kind = 2u;" in consumer
assert "prepared.source_kind=source.consumer_source_kind;" in draw
assert "prepared.source_kind = source.consumer_source_kind;" in draw
assert "g_v374_rt_source1_seen" in integrated
assert "diff_probe_a=%u diff_probe_b=%u source_kind=%u" in integrated
print("V374_SOURCE1_OWNER_SCOPED_LIFETIME_SOURCE_PASS runtime=OPEN pixel=OPEN")
