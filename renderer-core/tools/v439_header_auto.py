from pathlib import Path
p=Path('renderer-core/include/dsrrl/runtime/v438_one_shot_trace.hpp')
s=p.read_text()
old=' const bool now_f9=(GetAsyncKeyState(VK_F9)&0x8000)!=0;\n if(now_f9 && !was_f9 && !s.start_attempted.load())start_one_shot();\n was_f9=now_f9;'
assert s.count(old)==1
s=s.replace(old,' if(!s.start_attempted.load() && s.scene_draws.load()>=12u && frame>=30u)start_one_shot();')
s=s.replace(' static bool was_f9=false;\n','')
s=s.replace('CAPTURE_START F9:','CAPTURE_START AUTO:')
assert 'VK_F9' not in s
p.write_text(s)
