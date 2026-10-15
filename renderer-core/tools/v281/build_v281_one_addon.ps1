# V281 single .addon64 source-restored descendant of V279.
# Observes exact IA layout/VS/VB/IB state only at 2 FLVER x 2 MTD slots.
$ErrorActionPreference='Stop'
$parent='renderer-core/tools/v279/build_v279_one_addon.ps1'
$original=[IO.File]::ReadAllBytes($parent)
$text=[Text.Encoding]::UTF8.GetString($original)
$needle=" if(`$LASTEXITCODE -ne 0){throw 'V279 exact carrier oracle failed'}"
$ix=$text.IndexOf($needle,[StringComparison]::Ordinal)
if($ix -lt 0 -or $text.IndexOf($needle,$ix+1,[StringComparison]::Ordinal) -ge 0){
    throw 'V281 parent source overlay insertion anchor drift'
}
$extra=@'
 python renderer-core/tools/v281/apply_v281_flver_ia_vs_trace.py --apply
 if($LASTEXITCODE -ne 0){throw 'V281 exact IA VS source hook failed'}
 python renderer-core/tools/v281/apply_v281_flver_ia_vs_trace.py
 if($LASTEXITCODE -ne 0){throw 'V281 exact IA VS oracle failed'}
'@
$patched=$text.Replace($needle,$needle+"`n"+$extra.TrimEnd())
$parentBinary='DSRRL_V279_V272_METAL_LEATHER_LERP_CARRIER_WITNESS.addon64'
$output='DSRRL_V281_V272_FLVER_IA_VS_SOURCE_WITNESS.addon64'
try {
    python -m py_compile renderer-core/tools/v281/apply_v281_flver_ia_vs_trace.py
    if($LASTEXITCODE -ne 0){throw 'V281 python parse failure'}
    [IO.File]::WriteAllText($parent,$patched,[Text.UTF8Encoding]::new($false))
    & ./renderer-core/tools/v279/build_v279_one_addon.ps1
    if($LASTEXITCODE -ne 0){throw 'V281 native single addon failed'}
    if(!(Test-Path $parentBinary)){throw 'V281 parent binary absent'}
    $raw=[IO.File]::ReadAllBytes($parentBinary)
    $ascii=[Text.Encoding]::ASCII.GetString($raw)
    foreach($token in @('[DSRRL V281 IA VS]','[DSRRL V279 MATERIAL CARRIER]',
        '[DSRRL V275 MATERIAL DRAW]','[DSRRL V277 EXACT DXBC CAPTURE]')){
        if(!$ascii.Contains($token)){throw "V281 missing compiled marker $token"}
    }
    [IO.File]::WriteAllBytes($output,$raw)
    Write-Host "V281_SINGLE_ADDON_SHA256=$((Get-FileHash $output -Algorithm SHA256).Hash.ToLowerInvariant())"
    Write-Host 'V281_CONSTRUCTION_PASS_ONLY_RUNTIME_VERTEX_AND_PIXEL_OPEN'
} finally {
    [IO.File]::WriteAllBytes($parent,$original)
    git diff --exit-code HEAD -- renderer-core/integrated renderer-core/src renderer-core/include $parent
    if($LASTEXITCODE -ne 0){throw 'V281 source restitution failed'}
}
