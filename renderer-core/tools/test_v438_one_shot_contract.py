from pathlib import Path
p=Path('renderer-core')
s=(p/'src/runtime/pmetal_env_source_runtime.cpp').read_text()
a=(p/'integrated/integrated_addon.cpp').read_text()
assert 'WRAPPER_HOOKS_ACTIVE' in s and 'v438::SOURCE1_JOIN' in s
assert 'v438::ENV_ISSUED' in a and 'v438::on_present' in a
assert 'latest_hook_source(out)' in s
print('V438 source construction contracts PASS')
