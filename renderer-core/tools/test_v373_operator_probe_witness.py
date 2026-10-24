#!/usr/bin/env python3
"""V373: verify both operator probe ordinals are separately observable.

Static invariant only, NOT runtime activation or pixel equivalence.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
integrated = (root / "integrated/integrated_addon.cpp").read_text("utf-8")
runtime = (root / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
materializer = (root / "tools/materialize_v372_independent_bound_t11_composite.py").read_text("utf-8")
start = integrated.index("void v230_rt_probe_once(")
end = integrated.index("bool prepare_island_batch(", start)
cut = integrated[start:end]
for code in (
    "diffuse_probe_a = prepared->envdiffuse_resources.probe_a;",
    "diffuse_probe_b = prepared->envdiffuse_resources.probe_b;",
    "probe_a = prepared->env_resources.probe_a;",
    "probe_b = prepared->env_resources.probe_b;",
    "diff_probe_a=%u diff_probe_b=%u",
    "g_v230_rt_probe_seen.fetch_or(",
):
    assert cut.count(code) == 1, code
for code in (
    "diff_probe=%u b12=1 t11=PTDE t12=STOCK",
    "prepared.envdiffuse_resources.probe_a",
    "source.envdiffuse_a[0]",
    "source.bank_signature_a",
    "source.row_id_a",
):
    assert code in runtime, code
assert "prepare_envdiffuse_bound(" in materializer
assert "prepared.env_resources.probe_a" in materializer
assert materializer.index("prepare_envdiffuse_bound(") < materializer.index("prepared.env_resources.probe_a")
print("V373_INDEPENDENT_OPERATOR_PROBE_IDENTITY_WITNESS_SOURCE_PASS pixel=OPEN")
