from pathlib import Path
p=Path('renderer-core/tools/build_v438_one_shot.ps1')
s=p.read_text()
a='DSRRL_V438_Armor_Flicker_OneShot.addon64'
assert s.count(a)>=2
p.write_text(s.replace(a,'DSRRL_V439_Auto_Flicker_Safe.addon64'))
