from pathlib import Path
s=Path('renderer-core/src/runtime/pmetal_producer_state.cpp').read_text()
assert 'void pmetal_producer_state_begin(' in s
assert 'void pmetal_producer_state_publish(' in s
print('V236 protected source contract PASS')
