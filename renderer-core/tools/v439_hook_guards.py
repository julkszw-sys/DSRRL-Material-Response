from pathlib import Path
p=Path('renderer-core/src/runtime/pmetal_env_source_runtime.cpp')
s=p.read_text()
old='#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)\nstruct v438_wrapper_hook {'
assert s.count(old)==1
s=s.replace(old,'#if defined(DSRRL_V439_UNSAFE_WRAPPER_HOOKS)\nstruct v438_wrapper_hook {')
old='#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)\n    if(!v438_install_wrappers(base))'
assert s.count(old)==1
s=s.replace(old,'#if defined(DSRRL_V439_UNSAFE_WRAPPER_HOOKS)\n    if(!v438_install_wrappers(base))')
old='#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)\n    if(!v438_uninstall_wrappers())'
assert s.count(old)==1
s=s.replace(old,'#if defined(DSRRL_V439_UNSAFE_WRAPPER_HOOKS)\n    if(!v438_uninstall_wrappers())')
p.write_text(s)
