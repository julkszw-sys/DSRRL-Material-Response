#!/usr/bin/env python3
"""V470: preserve V359 hot-path audit and V438 tests before INFO log cleanup.

Patch only the known V438 generator's post-audit test command substitution;
never modify the original sources before their mandatory audits.
"""
from pathlib import Path
p=Path("renderer-core/tools/build_v438_one_shot.ps1")
s=p.read_text(encoding="utf-8")
needle="Replace-One 'python renderer-core/tools/test_v230_release_contract.py' 'python renderer-core/tools/test_v438_one_shot_contract.py'"
assert s.count(needle)==1 and "v470_release_info_cleanup_overlay.py" not in s,"V469 test-build lineage drift"
replacement="""Replace-One 'python renderer-core/tools/test_v230_release_contract.py' @'
python renderer-core/tools/test_v438_one_shot_contract.py
if ($LASTEXITCODE -ne 0) { throw 'V438 source contract failed before V470 post-audit cleanup' }
python renderer-core/tools/v470_release_info_cleanup_overlay.py
'@"""
p.write_text(s.replace(needle,replacement,1),encoding="utf-8")
print("V470 scheduled INFO-only cleanup AFTER original V359 hot-path audit and V438 contract")
