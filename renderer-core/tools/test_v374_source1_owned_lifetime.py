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
if 'DSRRL_V398_ACTOR_TRANSACTIONAL_SOURCE' in (root/'tools/build_v230_production.ps1').read_text():
    assert selector.count("pmetal_producer_state_begin(") == 2
    assert "#if !defined(DSRRL_V398_ACTOR_TRANSACTIONAL_SOURCE)" in selector
    assert "source_a, endpoints.a" in selector
    assert selector.index("source_a, endpoints.a") < publish
    assert "pmetal_producer_state_actor_tracked(material, epoch)" in src
else:
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
# V394 separates model LightBank transport from material response.
# Retain the old material-key contract only on pre-V394 builds; exact model
# instances must be distinguished by native owner pointer + complete FLVER SHA.
if 'DSRRL_V394_MODEL_SOURCE1' in (root/'tools/build_v230_production.ps1').read_text():
    assert "same_model(" in state
    assert "model_owner_instance" in state
    key=state[state.index("std::uint64_t model_key("):state.index("std::uint64_t source_key(")]
    for forbidden in ("material_slot", "raw_mtd_sha256", "route_index", "semantic_name_hash"):
        assert forbidden not in key
else:
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
