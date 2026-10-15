$ErrorActionPreference='Stop'
$parent='renderer-core/tools/v277/build_v277_one_addon.ps1'
$original=[IO.File]::ReadAllBytes($parent)
$text=[Text.Encoding]::UTF8.GetString($original)
$needle=" if(`$LASTEXITCODE -ne 0){throw 'V277 capture oracle failed'}"
$pos=$text.IndexOf($needle,[StringComparison]::Ordinal)
if($pos -lt 0 -or $text.IndexOf($needle,$pos+1,[StringComparison]::Ordinal) -ge 0){throw 'V278 parent insertion oracle failed'}
$extra=@'
 python renderer-core/tools/v278/apply_v278_r5_lerp_experiment.py --apply
 if($LASTEXITCODE -ne 0){throw 'V278 exact R5 overlay failed'}
 python renderer-core/tools/v278/apply_v278_r5_lerp_experiment.py
 if($LASTEXITCODE -ne 0){throw 'V278 exact R5 overlay oracle failed'}
'@
$patched=$text.Replace($needle,$needle+"`n"+$extra.TrimEnd())
$from='DSRRL_V277_V272_STABLE_LERP_DXBC_CAPTURE_DIAGNOSTIC.addon64'
$out='DSRRL_V278_V272_EXACT_LERP_R5_AB_EXPERIMENT.addon64'
try {
 python -m py_compile renderer-core/tools/v278/apply_v278_r5_lerp_experiment.py
 if($LASTEXITCODE -ne 0){throw 'V278 Python syntax error'}
 [IO.File]::WriteAllText($parent,$patched,[Text.UTF8Encoding]::new($false))
 & ./renderer-core/tools/v277/build_v277_one_addon.ps1
 if($LASTEXITCODE -ne 0){throw 'V278 parent native build failed'}
 if(!(Test-Path $from)){throw 'V278 parent addon missing'}
 $b=[IO.File]::ReadAllBytes($from)
 $ascii=[Text.Encoding]::ASCII.GetString($b)
 foreach($t in @('[DSRRL V277 EXACT DXBC CAPTURE]','[DSRRL V275 MATERIAL DRAW]','[DSRRL V278 R5 LERP AB]')){
    if(!$ascii.Contains($t)){throw "V278 missing native marker: $t"}
 }
 [IO.File]::WriteAllBytes($out,$b)
 Write-Host "V278_R5_AB_SHA256=$((Get-FileHash $out -Algorithm SHA256).Hash.ToLowerInvariant())"
 Write-Host "V278_EXACT_LERP_R5_AB_NATIVE_BUILD_PASS"
 Write-Host "V278_GPU_NORMAL_AND_PTDE_PIXEL_OPEN"
} finally {
 [IO.File]::WriteAllBytes($parent,$original)
 git diff --exit-code HEAD -- renderer-core/src renderer-core/integrated renderer-core/include $parent
 if($LASTEXITCODE -ne 0){throw 'V278 source restore failure'}
}
