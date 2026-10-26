from pathlib import Path
p=Path(__file__).resolve().parents[1]
h=(p/'include/dsrrl/runtime/pmetal_env_source_runtime.hpp').read_text()
s=(p/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
d=(p/'src/runtime/pmetal_envspec_draw_runtime.cpp').read_text()
g=(p/'integrated/integrated_addon.cpp').read_text()
assert 'native_producer_sequence = 0u' in h
assert 'next.native_producer_sequence = serial;' in s
assert d.count('source.native_producer_sequence;') == 2
assert 'native_producer_seq=%llu actor_owner=%p' in g
assert 'prepared.stable_envdiffuse_native_producer_sequence' in g
assert 'latest_hook_source(out)' in s
print('V400 provenance transport contract PASS')
