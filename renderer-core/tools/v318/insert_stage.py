from pathlib import Path
p=Path("renderer-core/tools/v258/build_v258_ptde_equipment_dormant_one_addon.ps1")
s=p.read_text(encoding="utf-8").replace("\r\n","\n")
anchor="V317 exact owner shader oracle failed"
assert s.count(anchor)==1 and "V318 staged V299" not in s
i=s.index("\n",s.index(anchor))+1
new=(" python renderer-core/tools/v318/stage_v299_source.py\n"
     " if($LASTEXITCODE -ne 0){throw 'V318 staged V299 guard failed'}\n")
p.write_text(s[:i]+new+s[i:],encoding="utf-8",newline="\n")
print("V318_V299_BUILD_INTEGRATION_READY")
