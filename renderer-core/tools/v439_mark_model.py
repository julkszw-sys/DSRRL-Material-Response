from pathlib import Path
p=Path('renderer-core/src/runtime/flver_engine_hooks.cpp')
s=p.read_text()
a='             v438::emit(v438::MODEL_SELECTOR,{model_addr,'
assert s.count(a)==1
p.write_text(s.replace(a,'             v438::state().scene_draws.fetch_add(1u,std::memory_order_relaxed);\n'+a,1))
