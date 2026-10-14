# V274 native diagnostic on immutable V272 equipment baseline.
# Read only actual physical GPU PS b0 on authenticated interior Lerp draws.
# No shader/material/CB payload/sidecar output changes; stable HemEnv untouched.
$ErrorActionPreference='Stop'
$baseline='ae9ac314d06ad8758e1aa7430cb236eef0eefe27'
$builder='renderer-core/tools/v258/build_v258_ptde_equipment_dormant_one_addon.ps1'
$original=[IO.File]::ReadAllText($builder).Replace([string][char]13+[char]10,[string][char]10)
$unique='V271 original DSR Fog/LS/Glow postcondition oracle failure'
$overlay=@'
 python renderer-core/tools/v274/apply_v274_lerp_actual_cb85_witness.py --apply
 if($LASTEXITCODE -ne 0){throw 'V274 Lerp-only GPU CB witness overlay failed'}
 python renderer-core/tools/v274/apply_v274_lerp_actual_cb85_witness.py
 if($LASTEXITCODE -ne 0){throw 'V274 Lerp-only GPU CB witness oracle failed'}
'@
$markerAt=$original.IndexOf($unique,[StringComparison]::Ordinal)
if($markerAt -lt 0 -or
   $original.IndexOf($unique,$markerAt+1,[StringComparison]::Ordinal) -ge 0){
    throw 'V274 preimage: unique V271 source oracle marker absent or duplicated'
}
$lineEnd=$original.IndexOf([char]10,$markerAt)
if($lineEnd -lt 0){throw 'V274 original builder injection line missing'}
$patched=$original.Insert($lineEnd+1,$overlay.TrimEnd()+[char]10)
$baselineAddon='DSRRL_V272_EQUIPMENT_PTDE_INTERIOR_BETA_ROUTING.addon64'
$out='DSRRL_V274_V272_EQUIPMENT_LERP_ACTUAL_CB85_WITNESS.addon64'
try {
    git diff --quiet $baseline HEAD -- renderer-core/src renderer-core/include renderer-core/integrated
    if($LASTEXITCODE -ne 0){throw 'V274 immutable V272 tracked renderer sources changed in Git'}
    git diff --quiet HEAD -- renderer-core/src renderer-core/include renderer-core/integrated
    if($LASTEXITCODE -ne 0){throw 'V274 refused nonpristine original renderer working tree'}
    python renderer-core/tools/v274/test_v274_actual_cb85_witness.py
    if($LASTEXITCODE -ne 0){throw 'V274 Lerp-only source negative oracles failed'}
    [IO.File]::WriteAllText($builder,$patched,[Text.UTF8Encoding]::new($false))
    & $builder
    if($LASTEXITCODE -ne 0){throw 'V274 V272 nested native full build failed'}
    if(!(Test-Path $baselineAddon)){throw 'V274 nested builder output absent'}
    $ascii=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($baselineAddon))
    foreach($mark in @(
        '[DSRRL V274 ACTUAL LERP CB0]',
        '[DSRRL V272 INTERIOR LERP B12 ROUTE]',
        '[DSRRL V258 PTDE GPU ISLAND DORMANT]',
        '[DSRRL V253 PTDE ENVSPEC RGBM]')){
        if(!$ascii.Contains($mark)){throw "V274 binary marker missing $mark"}
    }
    Copy-Item $baselineAddon $out -Force
    $sha=(Get-FileHash $out -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host "V274_V272_LERP_ACTUAL_CB85_WITNESS_SHA256=$sha"
    Write-Host 'V274_SOURCE_STABLE_V272_UNCHANGED=PASS'
    Write-Host 'V274_SHADER_DXBC_AND_CARRIERS_V272=UNMODIFIED'
    Write-Host 'V274_INTERIOR_LERP_DIAG_ONLY_BOUND_MAX96_GPU_CB_READS'
    Write-Host 'V274_RUNTIME_CB_READ_AND_PIXEL=OPEN'
} finally {
    [IO.File]::WriteAllText($builder,$original,[Text.UTF8Encoding]::new($false))
    if([IO.File]::ReadAllText($builder).Replace([string][char]13+[char]10,[string][char]10) -cne $original){
        throw 'V274 tracked source builder not restored byte-exact'
    }
    # Original V250/V253 builder temporarily overlays production sources.
    # Since entry tree was verified pristine, restore only the tracked
    # renderer sources that the native build changed, not user files.
    git restore --source=HEAD --worktree -- renderer-core/src renderer-core/include renderer-core/integrated
    if($LASTEXITCODE -ne 0){throw 'V274 temporary renderer source restoration failed'}
    git diff --exit-code HEAD -- renderer-core/src renderer-core/include renderer-core/integrated renderer-core/tools/v258/build_v258_ptde_equipment_dormant_one_addon.ps1
    if($LASTEXITCODE -ne 0){throw 'V274 native source not restored to immutable V272 HEAD'}
}
