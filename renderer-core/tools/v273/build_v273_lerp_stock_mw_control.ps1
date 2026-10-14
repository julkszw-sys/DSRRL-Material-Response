# V273: build V272 owner equipment addon, but isolate ONLY the R19
# Lerp MaterialWorkflow consumer by retaining stock DSR MW. Stable HemEnv
# files/PS/CB/resources and all equipment sidecars are exactly V272.
# Full original builder source, generation, and test chain is reused.
$ErrorActionPreference='Stop'
$baseline='ae9ac314d06ad8758e1aa7430cb236eef0eefe27'
$builder='renderer-core/tools/v258/build_v258_ptde_equipment_dormant_one_addon.ps1'
$original=[IO.File]::ReadAllText($builder).Replace([string][char]13+[char]10,[string][char]10)
$unique='V271 original DSR Fog/LS/Glow postcondition oracle failure'
$overlay=@'
 python renderer-core/tools/v273/apply_v273_lerp_stock_mw_control.py --apply
 if($LASTEXITCODE -ne 0){throw 'V273 Lerp-only source overlay failed'}
 python renderer-core/tools/v273/apply_v273_lerp_stock_mw_control.py
 if($LASTEXITCODE -ne 0){throw 'V273 exact Lerp materializer oracle failed'}
'@
$markerAt=$original.IndexOf($unique,[StringComparison]::Ordinal)
if($markerAt -lt 0 -or
   $original.IndexOf($unique,$markerAt+1,[StringComparison]::Ordinal) -ge 0){
    throw 'V273 preimage: V271 source oracle marker absent or duplicated'
}
$lineEnd=$original.IndexOf([char]10,$markerAt)
if($lineEnd -lt 0){throw 'V273 source builder insertion line missing EOL'}
$patched=$original.Insert($lineEnd+1,$overlay.TrimEnd()+[char]10)
$baselineAddon='DSRRL_V272_EQUIPMENT_PTDE_INTERIOR_BETA_ROUTING.addon64'
$out='DSRRL_V273_V272_EQUIPMENT_LERP_STOCK_DSR_MW_CONTROL.addon64'
try {
    # All tracked renderer source remains byte-identical to V272 in Git.
    git diff --quiet $baseline HEAD -- renderer-core/src renderer-core/include renderer-core/integrated renderer-core/shaders
    if($LASTEXITCODE -ne 0){throw 'Protected V272 renderer source changed in Git'}
    python renderer-core/tools/v273/test_v273_lerp_control.py
    if($LASTEXITCODE -ne 0){throw 'V273 source negative oracles failed'}
    [IO.File]::WriteAllText($builder,$patched,[Text.UTF8Encoding]::new($false))
    & $builder
    if($LASTEXITCODE -ne 0){throw 'V273 V272 native build failed'}
    if(!(Test-Path $baselineAddon)){throw 'V273 V272 output absent'}
    $ascii=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($baselineAddon))
    foreach($mark in @(
        '[DSRRL V273 LERP DSR MW CONTROL]',
        '[DSRRL V272 INTERIOR LERP B12 ROUTE]',
        '[DSRRL V258 PTDE GPU ISLAND DORMANT]',
        '[DSRRL V253 PTDE ENVSPEC RGBM]')){
        if(!$ascii.Contains($mark)){throw "V273 binary marker missing $mark"}
    }
    Copy-Item $baselineAddon $out -Force
    $sha=(Get-FileHash $out -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host "V273_V272_LERP_DSR_MW_CONTROL_SHA256=$sha"
    Write-Host 'V273_SOURCE_STABLE_V272_UNCHANGED=PASS'
    Write-Host 'V273_LERP_R19_STOCK_DSR_MATERIALWORKFLOW=COMPILED_CONTROL'
    Write-Host 'V273_PTDESPECRGB_EQUIPMENT_ROUTES=V272'
    Write-Host 'V273_RENDERER_RUNTIME_PIXEL=OPEN'
} finally {
    [IO.File]::WriteAllText($builder,$original,[Text.UTF8Encoding]::new($false))
    if([IO.File]::ReadAllText($builder).Replace([string][char]13+[char]10,[string][char]10) -cne $original){
        throw 'V273 original tracked builder restore mismatch'
    }
    git diff --exit-code -- renderer-core/src renderer-core/include renderer-core/integrated renderer-core/shaders
    if($LASTEXITCODE -ne 0){throw 'V273 transient renderer source not restored'}
}
