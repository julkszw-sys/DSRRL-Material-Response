from pathlib import Path
p=Path('renderer-core/tools/build_v438_one_shot.ps1')
s=p.read_text()
a='[DSRRL V438] WRAPPER_HOOKS_ACTIVE'
assert s.count(a)==1
p.write_text(s.replace(a,'[DSRRL V439] WRAPPER_HOOKS_DISABLED'))
