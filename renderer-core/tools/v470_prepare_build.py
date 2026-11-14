#!/usr/bin/env python3
"""V470: schedule INFO-only release cleanup AFTER mandatory hot-path audits.

Never modify sources before the V350/V359 original hot-path/source audit.
After generated source is materialized and audited, remove only INFO log
calls immediately before CMake compilation; do not change test code.
"""
from pathlib import Path
p=Path("renderer-core/tools/build_v438_one_shot.ps1")
s=p.read_text(encoding="utf-8")
needle='Replace-One "cmake --build build-v230-release --config Release --target dsrrl_renderer_core_integrated" "cmake --build build-v438-one-shot-diagnostic --config Release --target dsrrl_renderer_core_integrated"'
assert s.count(needle)==1 and "v470_release_info_cleanup_overlay.py" not in s,"V469 builder lineage drift"
addition=r"""
# V470: source audits and V438 source contract have already passed.
# This is a post-audit instrumentation-only cut; retain all audit preimages.
$buildV470='cmake --build build-v438-one-shot-diagnostic --config Release --target dsrrl_renderer_core_integrated'
$cutV470=@'
python renderer-core/tools/v470_release_info_cleanup_overlay.py
if ($LASTEXITCODE -ne 0) { throw 'V470 post-audit INFO-only cleanup failed' }
'@
Replace-One $buildV470 ($cutV470.TrimEnd() + "`n" + $buildV470)
"""
p.write_text(s.replace(needle,needle+"\n"+addition,1),encoding="utf-8")
print("V470 scheduled INFO-only log cleanup AFTER hot-path/source audits")
