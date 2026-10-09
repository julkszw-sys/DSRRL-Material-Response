#!/usr/bin/env python3
"""Tag exact R43 P_Metal hybrid source without touching PointLight."""
from pathlib import Path
p=Path("renderer-core/src/runtime/pmetal_env_source_runtime.cpp")
s=p.read_text()
needle='''    return true;
}

void pmetal_env_source_runtime::uninstall() noexcept'''
assert s.count(needle)==1
tag='''#if defined(DSRRL_PMETAL_R43_MAP_SOURCE_JOIN)
    reshade::log::message(
        reshade::log::level::info,
        "[DSRRL PMETAL R43 DSR HYBRID] source=PTDE_OR_NATIVE_DSR exact_bank_m99_default=ON extra_rows_m14_s14_m18_64=ON map_material_join=ON unchanged_PointLight=ON");
#endif
    return true;
}

void pmetal_env_source_runtime::uninstall() noexcept'''
p.write_text(s.replace(needle,tag,1))
print("PASS R43 DSR hybrid runtime marker")
