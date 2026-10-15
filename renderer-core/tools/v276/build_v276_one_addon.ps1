$ErrorActionPreference='Stop'
$parent='DSRRL_V275_V272_CLOTH_LEATHER_EXACT_DRAW_DIAGNOSTIC.addon64'
$out='DSRRL_V276_V272_CLOTH_LEATHER_BALANCED_LERP_DIAGNOSTIC.addon64'
& ./renderer-core/tools/v275/build_v275_one_addon.ps1
if($LASTEXITCODE -ne 0){throw 'V276 nested V275 native build failed'}
if(!(Test-Path $parent)){throw 'V276 parent output missing'}
$bytes=[IO.File]::ReadAllBytes($parent)
$marker=[Text.Encoding]::ASCII.GetString($bytes)
foreach($token in @('[DSRRL V275 MATERIAL DRAW]','v276_balanced=1','[DSRRL V276 COVERAGE]')){
    if(!$marker.Contains($token)){throw "V276 missing source marker $token"}
}
[IO.File]::WriteAllBytes($out,$bytes)
$sha=(Get-FileHash $out -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "V276_BALANCED_NATIVE_BINARY_SHA256=$sha"
Write-Host 'V276_28_X_1024_MTD_FAIR_COVERAGE_BUILD_PASS'
Write-Host 'V276_OWNER_GPU_CB_AND_PTDE_PIXEL_OPEN'
git diff --exit-code HEAD -- renderer-core/src renderer-core/include renderer-core/integrated
if($LASTEXITCODE -ne 0){throw 'V276 immutable V272 renderer source changed'}
