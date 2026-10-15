# V279 compile wraps the source-restoring V277 one-addon builder; no
# permanent integration/EXE/shader changes. Every V279 probe is read-only.
$ErrorActionPreference='Stop'
$parent='renderer-core/tools/v277/build_v277_one_addon.ps1'
$original=[IO.File]::ReadAllBytes($parent)
$text=[Text.Encoding]::UTF8.GetString($original)
$needle=" if(`$LASTEXITCODE -ne 0){throw 'V277 capture oracle failed'}"
$ix=$text.IndexOf($needle,[StringComparison]::Ordinal)
if($ix -lt 0 -or $text.IndexOf($needle,$ix+1,[StringComparison]::Ordinal) -ge 0){
    throw 'V279 source insertion anchor drift'
}
$extra=@'
 python renderer-core/tools/v279/apply_v279_material_carrier_trace.py --apply
 if($LASTEXITCODE -ne 0){throw 'V279 exact carrier source hook failed'}
 python renderer-core/tools/v279/apply_v279_material_carrier_trace.py
 if($LASTEXITCODE -ne 0){throw 'V279 exact carrier oracle failed'}
'@
$patched=$text.Replace($needle,$needle+"`n"+$extra.TrimEnd())
$parent_binary='DSRRL_V277_V272_STABLE_LERP_DXBC_CAPTURE_DIAGNOSTIC.addon64'
$output='DSRRL_V279_V272_METAL_LEATHER_LERP_CARRIER_WITNESS.addon64'
try {
    python -m py_compile renderer-core/tools/v279/apply_v279_material_carrier_trace.py
    if($LASTEXITCODE -ne 0){throw 'V279 python parse fail'}
    [IO.File]::WriteAllText($parent,$patched,[Text.UTF8Encoding]::new($false))
    & ./renderer-core/tools/v277/build_v277_one_addon.ps1
    if($LASTEXITCODE -ne 0){throw 'V279 Windows native one-addon build failed'}
    if(!(Test-Path $parent_binary)){throw 'V279 parent binary missing'}
    $b=[IO.File]::ReadAllBytes($parent_binary)
    $a=[Text.Encoding]::ASCII.GetString($b)
    foreach($token in @('[DSRRL V279 MATERIAL CARRIER]',
         '[DSRRL V275 MATERIAL DRAW]','[DSRRL V277 EXACT DXBC CAPTURE]')){
        if(!$a.Contains($token)){throw "V279 compiled marker missing: $token"}
    }
    [IO.File]::WriteAllBytes($output,$b)
    Write-Host "V279_CARRIER_SINGLE_ADDON_SHA256=$((Get-FileHash $output -Algorithm SHA256).Hash.ToLowerInvariant())"
    Write-Host 'V279_NATIVE_EXACT_CARRIER_CONSTRUCTION_PASS'
    Write-Host 'V279_RUNTIME_AND_PTDE_PIXEL_OPEN'
} finally {
    [IO.File]::WriteAllBytes($parent,$original)
    git diff --exit-code HEAD -- renderer-core/integrated renderer-core/src renderer-core/include $parent
    if($LASTEXITCODE -ne 0){throw 'V279 source restitution failed'}
}
