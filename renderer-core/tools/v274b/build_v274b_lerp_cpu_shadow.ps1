# V274B: V274 failed to map b0 on DSR deferred D3D11 context (76/76).
# Read-only ReShade CPU CB writer shadow, without touching stable V272/PS.
$ErrorActionPreference='Stop'
$baseline='ae9ac314d06ad8758e1aa7430cb236eef0eefe27'
$originalBuilder='renderer-core/tools/v274/build_v274_lerp_actual_cb85_witness.ps1'
$head='renderer-core/include/dsrrl/runtime/v274b_cb_shadow.hpp'
$original=[IO.File]::ReadAllText($originalBuilder).Replace([string][char]13+[char]10,[string][char]10)
$anchor='V274 Lerp-only GPU CB witness oracle failed'
$overlay=@'
 python renderer-core/tools/v274b/apply_v274b_deferred_cb0_shadow.py --apply
 if($LASTEXITCODE -ne 0){throw 'V274B CPU producer callbacks overlay failed'}
 python renderer-core/tools/v274b/apply_v274b_deferred_cb0_shadow.py
 if($LASTEXITCODE -ne 0){throw 'V274B exact CPU producer callbacks oracle failed'}
'@
$at=$original.IndexOf($anchor,[StringComparison]::Ordinal)
if($at -lt 0 -or $original.IndexOf($anchor,$at+1,[StringComparison]::Ordinal) -ge 0){
    throw 'V274B exact V274 builder preimage absent or duplicate'
}
$end=$original.IndexOf([char]10,$at)
if($end -lt 0){throw 'V274B missing V274 injection line ending'}
$patched=$original.Insert($end+1,$overlay.TrimEnd()+[char]10)
$v274Addon='DSRRL_V274_V272_EQUIPMENT_LERP_ACTUAL_CB85_WITNESS.addon64'
$out='DSRRL_V274B_V272_EQUIPMENT_LERP_DEFERRED_CB0_SHADOW.addon64'
try {
    git diff --quiet $baseline HEAD -- renderer-core/src renderer-core/include renderer-core/integrated renderer-core/shaders
    if($LASTEXITCODE -ne 0){throw 'V274B protected V272 production source diverged'}
    git diff --quiet HEAD -- renderer-core/src renderer-core/include renderer-core/integrated renderer-core/shaders
    if($LASTEXITCODE -ne 0){throw 'V274B refused dirty source before native build'}
    if(Test-Path $head){throw 'V274B refused to overwrite existing include header'}
    python renderer-core/tools/v274b/test_v274b_deferred_cb0_shadow.py
    if($LASTEXITCODE -ne 0){throw 'V274B five static negative oracles failed'}
    [IO.File]::WriteAllText($originalBuilder,$patched,[Text.UTF8Encoding]::new($false))
    & $originalBuilder
    if($LASTEXITCODE -ne 0){throw 'V274B full one-addon native build failed'}
    if(!(Test-Path $v274Addon)){throw 'V274B V274 nested output missing'}
    $ascii=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($v274Addon))
    foreach($marker in @(
        '[DSRRL V274 ACTUAL LERP CB0]',
        '[DSRRL V274B DEFERRED CB0 CPU SHADOW]',
        '[DSRRL V272 INTERIOR LERP B12 ROUTE]',
        '[DSRRL V253 PTDE ENVSPEC RGBM]')){
        if(!$ascii.Contains($marker)){throw "V274B compiled addon missing marker $marker"}
    }
    Copy-Item $v274Addon $out -Force
    Write-Host "V274B_LERP_DEFERRED_CPU_SHADOW_SHA256=$((Get-FileHash $out -Algorithm SHA256).Hash.ToLowerInvariant())"
    Write-Host 'V274B_WINDOWS_NATIVE_ONE_ADDON_CONSTRUCTION=PASS'
    Write-Host 'V274B_STABLE_V272_RENDERER_ALL_OPERATORS_PROTECTED'
    Write-Host 'V274B_ACTUAL_DEFERRED_CPU_WRITER_SHADOW=RUNTIME_OPEN'
    Write-Host 'V274B_PIXEL=OPEN'
} finally {
    [IO.File]::WriteAllText($originalBuilder,$original,[Text.UTF8Encoding]::new($false))
    if([IO.File]::ReadAllText($originalBuilder).Replace([string][char]13+[char]10,[string][char]10) -cne $original){
        throw 'V274B original V274 tracked builder not restored'
    }
    if(Test-Path $head){Remove-Item $head -Force}
    git restore --source=HEAD --worktree -- renderer-core/src renderer-core/include renderer-core/integrated
    if($LASTEXITCODE -ne 0){throw 'V274B temporary source restore command failed'}
    git diff --exit-code HEAD -- renderer-core/src renderer-core/include renderer-core/integrated renderer-core/tools/v274/build_v274_lerp_actual_cb85_witness.ps1
    if($LASTEXITCODE -ne 0){throw 'V274B protected tracked renderer source restore mismatch'}
    if(Test-Path $head){throw 'V274B shadow include transient header leaked'}
}
