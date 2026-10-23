#!/usr/bin/env python3
"""V350 scope-shell root-cause experiment. SOURCE guard, not runtime proof."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
h=(root/"renderer-core/include/dsrrl/runtime/qpc_causality_bisect.hpp").read_text(encoding="utf-8")
builder=(root/"renderer-core/tools/build_v350_qpc_shell_no_clock_one_addon.ps1").read_text(encoding="utf-8")
source=[
"renderer-core/src/runtime/flver_engine_hooks.cpp",
"renderer-core/src/runtime/flver_identity_registry.cpp",
"renderer-core/src/runtime/material_resource_draw_runtime.cpp",
]
for p in source:
    s=(root/p).read_text(encoding="utf-8")
    assert '#include "dsrrl/runtime/qpc_causality_bisect.hpp"' in s,p
    assert "qpc_bisect::scope " in s,p
assert "#if defined(DSRRL_QPC_SCOPE_SHELL_NO_CLOCK)" in h
shell=h.split("#if defined(DSRRL_QPC_SCOPE_SHELL_NO_CLOCK)\n// Code-shape isolation:",1)[1].split("#else\nclass scope {",1)[0]
assert "QueryPerformanceCounter" not in shell
assert "fetch_add(" not in shell
assert "update_max(" not in shell
assert "g_mode.load(std::memory_order_relaxed)" in shell
assert "if (mode_ == 0u) return;" in shell
assert "g_tls_qpc_sink = " in shell
assert "DSRRL_QPC_SCOPE_SHELL_NO_CLOCK" in builder
assert "DSRRL_FLVER_SELECTOR_PROFILE=OFF" in builder
assert "DSRRL_POINTLIGHT_PROFILE=OFF" in builder
assert "DSRRL_V350_RELEASE_SKIP_HOT_OBSERVERS" in builder
assert "DSRRL_V350_RC1_QPC_SCOPE_SHELL.addon64" in builder
assert "v350_rc1_qpc_shell_no_clock" in builder
assert "[DSRRL V350 PTDE LERP RGB]" in builder
print("V350 QPC SCOPE SHELL SOURCE PASS: same stage entry points, clock/RMW compiled out, PTDE preserved")
