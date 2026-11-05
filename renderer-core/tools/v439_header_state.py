from pathlib import Path
p=Path('renderer-core/include/dsrrl/runtime/v438_one_shot_trace.hpp')
s=p.read_text()
a='env_draws{0},marks{0};'
assert s.count(a)==1
p.write_text(s.replace(a,'env_draws{0},marks{0},scene_draws{0};'))
