from pathlib import Path
p=Path(__file__).resolve().parents[1]/'src/runtime/pmetal_envspec_draw_runtime.cpp'
s=p.read_text('utf-8')
assert '[DSRRL V378 NONMETAL PROBE CUT]' in s
assert 'shadow_env_a_ready?1u:0u' in s
assert 'shadow_env_b_ready?1u:0u' in s
assert 't.pack_ready?1u:0u' in s
assert 't.sampler_ready?1u:0u' in s
print('V378 source-level probe cut verified; runtime and pixels OPEN')
