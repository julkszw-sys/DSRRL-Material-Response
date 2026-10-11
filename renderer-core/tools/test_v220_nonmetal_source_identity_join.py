#!/usr/bin/env python3
"""Guard generated V220 exact nonmetal producer/consumer join."""
import importlib.util
from pathlib import Path

p = Path(__file__).with_name("apply_v220_nonmetal_source_identity_join.py")
spec = importlib.util.spec_from_file_location("v220", p)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

assert len(m.CHANGES) == 4
assert "exact.actual_material_exact = false" in m.HELPER

# Simulate the *materialized* R43 zero-selector donor, NOT the tracked
# pre-materialization selector (which contains an extra begin call).
donor = (
    "bool exact_envspec36_or_pmetal_source() {}\n"
    "bool has_full_owner() { return sidecar_20261010::exact_nonmetal_material(material); }\n"
    "bool retail_lightbank_record_index(\n"
    + "\n".join(before for before, _, _ in m.CHANGES)
    + "\nif (!exact_pmetal_material_selection(material)) return false;\n"
    + "if (latest_hook_source(out)) return true;\n"
    + "source_a_decode_ok;\n"
)
after = m.patch(donor)
assert m.patch(after) == after, "not idempotent"
assert after.count("v220_exact_nonmetal_source_key(") == 5
assert after.count("if (latest_hook_source(out))") == 1
assert after.count("if (!exact_pmetal_material_selection(material))") == 1
for original, replacement, expected in m.CHANGES:
    assert after.count(replacement) == expected
    assert after.count(original) == 0
assert "pmetal_producer_state_begin(" in after
print("V220_GENERATED_R43_ZERO_SELECTOR_TEST_PASS")
print("V220_PMETAL_FALLBACK_BYTE_PATTERN_UNCHANGED=TRUE")
