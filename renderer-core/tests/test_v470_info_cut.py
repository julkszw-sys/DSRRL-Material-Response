#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
src=(root/"tools/v470_release_info_cleanup_overlay.py").read_text(encoding="utf-8")
p=(root/"tools/v470_prepare_build.py").read_text(encoding="utf-8")
n=(root/"tools/v470_name_binary.py").read_text(encoding="utf-8")
for term in ("materialize_v230_release_cleanup.py","reshade::log::level::info,","reshade::log::level::warning",
             "reshade::log::level::error","V350 PTDE LERP RGB","GPU RESIDENT RC1","DSRRL BUILD_ID",
             'assert total>=25', 'src").rglob("*.cpp")'):
 assert term in src,term
assert "v470_release_info_cleanup_overlay.py" in p
assert "audit_active_islands_hotpath.py" in p
assert "DSRRL_V470_RX34_Focused_LogLowOverhead.addon64" in n
for forbidden in ("DrawIndexed(","set_lightbank(","shader_patch(","DSRRL_V230_RUNTIME_TELEMETRY=OFF"):
 assert forbidden not in src
print("V470 source-contract PASS; source-only INFO suppression, no FPS claim")
