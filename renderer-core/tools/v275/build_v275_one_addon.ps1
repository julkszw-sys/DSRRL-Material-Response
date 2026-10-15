# V275: immutable V272 + V274C CPU source witness + V275 exact Cloth/Leather
# physical bound DX11 PS/t1/t10/RT0/RT1 and planned one-addon mutations.
# All extra changes are transient, restored by nested original builder.
$ErrorActionPreference='Stop'
$base='ae9ac314d06ad8758e1aa7430cb236eef0eefe27'
$upstream='renderer-core/tools/v274b/build_v274b_lerp_cpu_shadow.ps1'
$upstreamBytes=[IO.File]::ReadAllBytes($upstream)
$upstreamSource=[Text.Encoding]::UTF8.GetString($upstreamBytes).Replace("`r`n","`n")
$anchor="V274B exact CPU producer callbacks oracle failed"
$position=$upstreamSource.IndexOf($anchor,[StringComparison]::Ordinal)
if($position -lt 0 -or $upstreamSource.IndexOf($anchor,$position+1,[StringComparison]::Ordinal) -ge 0){throw 'V275 upstream V274B insertion oracle mismatch'}
$eol=$upstreamSource.IndexOf([char]10,$position)
if($eol -lt 0){throw 'V275 missing source newline'}
$overlay=@'
 python renderer-core/tools/v275/apply_v275_material_draw_trace.py --apply
 if($LASTEXITCODE -ne 0){throw 'V275 source overlay apply failed'}
 python renderer-core/tools/v275/apply_v275_material_draw_trace.py
 if($LASTEXITCODE -ne 0){throw 'V275 source overlay verification failed'}
'@
$patched=$upstreamSource.Insert($eol+1,$overlay.TrimEnd()+[char]10)
$header='renderer-core/include/dsrrl/runtime/v275_material_draw_trace.hpp'
$source='DSRRL_V274C_V272_EQUIPMENT_LERP_FOG_LS_CB12_20_WITNESS.addon64'
$output='DSRRL_V275_V272_CLOTH_LEATHER_EXACT_DRAW_DIAGNOSTIC.addon64'
try {
    git diff --quiet $base HEAD -- renderer-core/src renderer-core/include renderer-core/integrated renderer-core/shaders
    if($LASTEXITCODE -ne 0){throw 'V275 immutable V272 tracked renderer source changed'}
    git diff --quiet HEAD -- renderer-core/src renderer-core/include renderer-core/integrated renderer-core/shaders
    if($LASTEXITCODE -ne 0){throw 'V275 dirty source tree refused'}
    if(Test-Path $header){throw 'V275 untracked transient header collision'}
    python -m py_compile renderer-core/tools/v275/apply_v275_material_draw_trace.py
    if($LASTEXITCODE -ne 0){throw 'V275 Python syntax error'}
    [IO.File]::WriteAllText($upstream,$patched,[Text.UTF8Encoding]::new($false))
    & ./renderer-core/tools/v274c/build_v274c_lerp_fog_ls_witness.ps1
    if($LASTEXITCODE -ne 0){throw 'V275 Windows integrated addon build failed'}
    if(!(Test-Path $source)){throw 'V275 parent output absent'}
    $bytes=[IO.File]::ReadAllBytes($source)
    $ascii=[Text.Encoding]::ASCII.GetString($bytes)
    foreach($m in @(
       '[DSRRL V275 MATERIAL DRAW]',
       '[DSRRL V274C ATMOS CB0]',
       '[DSRRL V274B DEFERRED CB0 CPU SHADOW]',
       '[DSRRL V272 INTERIOR LERP B12 ROUTE]',
       '[DSRRL V253 PTDE ENVSPEC RGBM]')){
        if(!$ascii.Contains($m)){throw "V275 marker missing from single addon: $m"}
    }
    [IO.File]::WriteAllBytes($output,$bytes)
    Write-Host "V275_BINARY_SHA256=$((Get-FileHash $output -Algorithm SHA256).Hash.ToLowerInvariant())"
    Write-Host "V275_EXACT_27_MATERIAL_TRACE_BINARY_PASS"
    Write-Host "V275_GPU_PIXELS_UNOBSERVED_REQUIRES_AUTHENTIC_CAPTURE"
} finally {
    [IO.File]::WriteAllBytes($upstream,$upstreamBytes)
    if(!(Compare-Object -ReferenceObject $upstreamBytes -DifferenceObject ([IO.File]::ReadAllBytes($upstream)) -SyncWindow 0 | Measure-Object).Count -eq 0){
        throw 'V275 upstream wrapper not restored'
    }
    if(Test-Path $header){Remove-Item $header -Force}
    git restore --source=HEAD --worktree -- renderer-core/src renderer-core/include renderer-core/integrated
    if($LASTEXITCODE -ne 0){throw 'V275 renderer source restore fail'}
    git diff --exit-code HEAD -- renderer-core/src renderer-core/include renderer-core/integrated $upstream
    if($LASTEXITCODE -ne 0){throw 'V275 immutable source changed after build'}
    if(Test-Path $header){throw 'V275 header leaked'}
}
