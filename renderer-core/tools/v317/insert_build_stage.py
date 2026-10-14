#!/usr/bin/env python3
from pathlib import Path
p=Path("renderer-core/tools/v258/build_v258_ptde_equipment_dormant_one_addon.ps1")
s=p.read_text(encoding="utf-8").replace("\r\n","\n")
anchor="V272 interior Lerp b12 source route oracle failed"
assert s.count(anchor)==1 and "V317 exact owner shader" not in s
i=s.index("\n",s.index(anchor))+1
insert=(" python renderer-core/tools/v317/apply_v317_owner_v286_lerp.py --apply\n"
        " if($LASTEXITCODE -ne 0){throw 'V317 exact owner shader failed'}\n"
        " python renderer-core/tools/v317/apply_v317_owner_v286_lerp.py\n"
        " if($LASTEXITCODE -ne 0){throw 'V317 exact owner shader oracle failed'}\n")
p.write_text(s[:i]+insert+s[i:],encoding="utf-8",newline="\n")
assert p.read_text().count("python renderer-core/tools/v317/apply_v317_owner_v286_lerp.py --apply")==1
print("V317_EXACT_V272_BUILD_STAGE_INSERT_PASS")
