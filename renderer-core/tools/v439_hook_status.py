from pathlib import Path
p=Path('renderer-core/src/runtime/pmetal_env_source_runtime.cpp')
s=p.read_text()
old='#endif\n    return true;\n}\n\nbool uninstall_envspec_source_hooks() noexcept'
new='''#endif
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
    reshade::log::message(reshade::log::level::warning,
      "[DSRRL V439] WRAPPER_HOOKS_DISABLED crash_isolation=ON");
#endif
    return true;
}

bool uninstall_envspec_source_hooks() noexcept'''
assert s.count(old)==1
p.write_text(s.replace(old,new,1))
