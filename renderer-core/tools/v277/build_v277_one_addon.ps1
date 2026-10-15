# V277 source-only diagnostic overlay: exactly one V272-origin ReShade addon.
# The V275 -> V274C -> V274B nested pipeline restores integrated_addon.cpp.
$ErrorActionPreference='Stop'
$parent='renderer-core/tools/v275/build_v275_one_addon.ps1'
$original=[IO.File]::ReadAllBytes($parent)
$text=[Text.Encoding]::UTF8.GetString($original)
$needle=" if(`$LASTEXITCODE -ne 0){throw 'V275 source overlay verification failed'}"
if($text.IndexOf($needle,[StringComparison]::Ordinal) -lt 0 -or $text.IndexOf($needle,$text.IndexOf($needle,[StringComparison]::Ordinal)+1,[StringComparison]::Ordinal) -ge 0){throw 'V277 V275 parent anchor drift'}
$extra=@'
 python renderer-core/tools/v277/apply_v277_capture.py --apply
 if($LASTEXITCODE -ne 0){throw 'V277 shader source capture overlay failed'}
 python renderer-core/tools/v277/apply_v277_capture.py
 if($LASTEXITCODE -ne 0){throw 'V277 capture oracle failed'}
'@
$patched=$text.Replace($needle,$needle+"`n"+$extra.TrimEnd())
$source='DSRRL_V276_V272_CLOTH_LEATHER_BALANCED_LERP_DIAGNOSTIC.addon64'
$out='DSRRL_V277_V272_STABLE_LERP_DXBC_CAPTURE_DIAGNOSTIC.addon64'
try {
    python -m py_compile renderer-core/tools/v277/apply_v277_capture.py
    if($LASTEXITCODE -ne 0){throw 'V277 Python parser failure'}
    [IO.File]::WriteAllText($parent,$patched,[Text.UTF8Encoding]::new($false))
    & ./renderer-core/tools/v276/build_v276_one_addon.ps1
    if($LASTEXITCODE -ne 0){throw 'V277 native addon build failed'}
    if(!(Test-Path $source)){throw 'V277 parent output missing'}
    $bytes=[IO.File]::ReadAllBytes($source)
    $all=[Text.Encoding]::ASCII.GetString($bytes)
    foreach($token in @('[DSRRL V275 MATERIAL DRAW]','v276_balanced=1',
      '[DSRRL V277 EXACT DXBC CAPTURE]','DSRRL_V277_CAPTURE')){
      if(!$all.Contains($token)){throw "V277 missing marker: $token"}
    }
    [IO.File]::WriteAllBytes($out,$bytes)
    Write-Host "V277_BINARY_SHA256=$((Get-FileHash $out -Algorithm SHA256).Hash.ToLowerInvariant())"
    Write-Host 'V277_NATIVE_EXACT_DXBC_CAPTURE_CONSTRUCTION_PASS'
    Write-Host 'V277_RUNTIME_PIXEL_OPEN'
} finally {
    [IO.File]::WriteAllBytes($parent,$original)
    if(([IO.File]::ReadAllBytes($parent).Length -ne $original.Length)){
        throw 'V277 parent script not restored'}
    git diff --exit-code HEAD -- renderer-core/integrated renderer-core/src renderer-core/include $parent
    if($LASTEXITCODE -ne 0){throw 'V277 source not restored'}
}
