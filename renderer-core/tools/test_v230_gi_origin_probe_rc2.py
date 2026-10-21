from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'src/runtime/pmetal_envspec_draw_runtime.cpp').read_text()
assert '[DSRRL V358 PROBE ORIGIN]' in s
assert '[DSRRL V357 RESOURCE SOURCE]' in s
assert '[DSRRL V354 PROBE REJECT]' in s
assert 'mr.family == material_response::diffuse_v1_family::hemenvlerp' in (r/'src/operators/env_spec/pmetal_rgba_materializer.cpp').read_text()
print('PR486 GI SOURCE CONTRACT PASS')
