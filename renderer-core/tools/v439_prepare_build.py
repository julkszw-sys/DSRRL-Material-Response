from pathlib import Path
p=Path('renderer-core/tools/v438_apply_draw_part_b.py')
s=p.read_text()
a='''\nimport v439_header_state
import v439_header_auto
import v439_mark_model
import v439_hook_guards
import v439_hook_status
'''
assert s.endswith('\n')
assert 'import v439_hook_status' not in s
p.write_text(s+a)
print('V439 auto and safe source observers scheduled after V438')
