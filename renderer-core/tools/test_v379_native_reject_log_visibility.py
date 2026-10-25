from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=(root/"tools/materialize_v354_exact_resource_resnapshot.py").read_text("utf-8")
assert '"[DSRRL V354 SPEC REJECT]' in p
assert '"[DSRRL V354 PROBE REJECT]' in p
assert 'native_t12_bound=%u native_t14_bound=%u' in p
assert 'native_a_known=%u native_a_probe=%u' in p
assert 'native_t1_bound=%u' in p
assert 'sidecar_ready=%u' in p
assert p.count("reshade::log::level::warning,line);")==2
assert 'resolution=NOT_AUTHORIZED_RELEASE_BLOCK' in p
assert 'material_resources_.probe_exact_specular_companion(context)' in p
assert 'v354_native_probe_for_view(native_view[0],probe_a)' in p
print("V379_EXACT_REJECT_WARN_VISIBLE_CONTRACT_PASS")
