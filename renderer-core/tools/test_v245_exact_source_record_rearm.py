#!/usr/bin/env python3
"""Static source oracle for exact V245 producer-payload repair."""
import importlib.util
from pathlib import Path

root = Path(__file__).resolve().parents[2]
script = Path(__file__).with_name("apply_v245_exact_source_record_rearm.py")
spec = importlib.util.spec_from_file_location("v245", script)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
src = (root / mod.SOURCE).read_text(encoding="utf-8")
test = (root / mod.TEST).read_text(encoding="utf-8")

# The tracked input is intentionally V236-free. Mimic the existing V236
# per-slot locking overlay in memory, without writing production source.
v236_rewrites = (
    ("""    auto &slot = sync_slot_for(key);

    // Invalidate before source resolution.""",
     """    auto &slot = sync_slot_for(key);
    std::lock_guard<std::mutex> lock(slot.mutex);

    // Invalidate before source resolution."""),
    ("""    auto &slot = sync_slot_for(key);

    // Same exact material/source/epoch:""",
     """    auto &slot = sync_slot_for(key);
    std::lock_guard<std::mutex> lock(slot.mutex);

    // Same exact material/source/epoch:"""),
    ("""        return;
    }

    std::lock_guard<std::mutex> lock(slot.mutex);
    slot.valid.store(""",
     """        return;
    }

    slot.valid.store("""),
)
for old, new in v236_rewrites:
    assert src.count(old) == 1, "V236 preimage drift"
    src = src.replace(old, new, 1)
# Unit test and producer must be exact, idempotent overlays.
patched = mod.patch_source(src)
assert mod.patch_source(patched) == patched
changed_test = mod.patch_test(test)
assert mod.patch_test(changed_test) == changed_test
assert changed_test.count("return 43;") == 1
for field in (
    "envdiffuse_a", "envdiffuse_b", "envdiffuse_linear_valid",
    "phn_k135", "phn_k135_valid"):
    assert "a." + field in mod.NEW_SAME
    assert "source." + field in mod.NEW_KEY
for needle in (
    "std::lock_guard<std::mutex> lock(slot.mutex)",
    "same_material(slot.record.material, material)",
    "same_source_payload(slot.record.source, g_record.source)",
    "slot.record.generation == g_record.generation",
    "slot.record = g_record;",
):
    assert needle in patched, needle
# No selector material identity/route change, no DXBC, resource, or draw changes.
assert patched.replace(mod.NEW_SAME, mod.OLD_SAME, 1).replace(
    mod.NEW_KEY, mod.OLD_KEY, 1).replace(mod.NEW_REARM,
    mod.OLD_REARM, 1) == src
# Mandatory exact negative/positive control even if CTest uses MSVC Release
# and NDEBUG removes all ordinary assert() calls.
for exit_code in range(41, 50):
    assert f"return {exit_code};" in changed_test
print("V245_PRODUCER_PAYLOAD_AND_REARM_SOURCE_ORACLE_PASS")
print("V245_RENDERER_SHADER_RESOURCE_AND_DRAW_SOURCE_UNCHANGED_PASS")
