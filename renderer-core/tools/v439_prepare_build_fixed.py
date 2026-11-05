from pathlib import Path
p=Path('renderer-core/tools/v439_hook_guards.py')
s=p.read_text()
assert s.count('// Exact 17-byte')==2
s=s.replace('// Exact 17-byte','struct v438_wrapper_hook {')
p.write_text(s)
import v439_prepare_build
print('V439 split original wrapper guard and auto prep PASS')
