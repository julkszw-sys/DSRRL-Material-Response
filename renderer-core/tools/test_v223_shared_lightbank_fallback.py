#!/usr/bin/env python3
"""V223 deterministic source-overlay controls, P_Metal parity and fail-open."""
import importlib.util
from pathlib import Path

p = Path(__file__).with_name("apply_v223_shared_lightbank_fallback.py")
spec = importlib.util.spec_from_file_location("v223", p)
v223 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(v223)

fixture = (
    'bool exact_envspec36_or_pmetal_source() {}\n'
    'bool full_owner() { return sidecar_20261010::exact_nonmetal_material(material); }\n'
    'const bool v220_nonmetal_join = pmetal_producer_state_latest(material,epoch,out);\n'
    'if (v220_nonmetal_join) { return true; }\n'
    + v223.ANCHOR
    + '    if (!exact_pmetal_material_selection(material)) return false;\n'
    + '#endif\n'
    + 'if (latest_hook_source(out)) { return true; }\n'
)
result = v223.patch(fixture)
assert v223.patch(result) == result
assert result.count("[DSRRL V223 SHARED LIGHTBANK]") == 1
assert result.count('if (latest_hook_source(out)) {') == 1
assert result.count("latest_hook_source(shared_source)") == 1
assert result.index("const bool v220_nonmetal_join =") < result.index(
    "latest_hook_source(shared_source)")
assert result.index("latest_hook_source(shared_source)") < result.index(
    "if (!exact_pmetal_material_selection(material))")
assert result.index("if (!exact_pmetal_material_selection(material))") < result.index(
    "if (latest_hook_source(out)) {")
assert 'sidecar_20261010::exact_nonmetal_material(material)' in v223.INSERT
assert "if (shared_available) {\n            out = shared_source;" in result
assert "telemetry::hot_count(g_consumer_ok)" in result
assert "return true;" in v223.INSERT
assert "next=standard_source_probe_spec_gates pixel=OPEN" in result
assert "32u" in result
assert "source.valid" not in result
for mutated in (
    fixture.replace("const bool v220_nonmetal_join =", "const bool old_join ="),
    fixture.replace("if (latest_hook_source(out)) {", "if (false) {"),
    fixture.replace("Unkeyed R43 latest_hook_source may only serve original exact P_Metal.",
                    "Broken guard."),
):
    try:
        v223.patch(mutated)
    except RuntimeError:
        continue
    raise AssertionError("V223 mutation detector accepted invalid fixture")
print("V223_SHARED_LIGHTBANK_SOURCE_FALLBACK_ORACLE_PASS")
print("V223_NEGATIVE_MATERIAL_AND_PMETAL_FALLBACK_GUARDS_PASS")
print("V223_RUNTIME_AND_PTDE_PIXEL_OPEN")
