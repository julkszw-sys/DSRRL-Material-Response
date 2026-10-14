# V274C: V272 -> V274 -> V274B same one-addon native build, logging
# only cb0[12] Fog and cb0[13..20] LightScattering from 96 existing
# deferred-context material-prepared CPU shadow snapshots.
$ErrorActionPreference='Stop'
$base='ae9ac314d06ad8758e1aa7430cb236eef0eefe27'
$builder='./renderer-core/tools/v274b/build_v274b_lerp_cpu_shadow.ps1'
$from='DSRRL_V274B_V272_EQUIPMENT_LERP_DEFERRED_CB0_SHADOW.addon64'
$to='DSRRL_V274C_V272_EQUIPMENT_LERP_FOG_LS_CB12_20_WITNESS.addon64'
git diff --quiet $base HEAD -- renderer-core/src renderer-core/include renderer-core/integrated renderer-core/shaders
if($LASTEXITCODE -ne 0){throw 'V274C immutable V272 sources differ'}
python renderer-core/tools/v274c/test_v274c_atmos_cb0_log.py
if($LASTEXITCODE -ne 0){throw 'V274C Fog and LightScattering read-only oracle FAIL'}
& $builder
if($LASTEXITCODE -ne 0){throw 'V274C nested V274B Windows build failed'}
if(!(Test-Path $from)){throw 'V274C original native build artifact missing'}
$bytes=[IO.File]::ReadAllBytes($from)
$body=[Text.Encoding]::ASCII.GetString($bytes)
foreach($m in @(
    '[DSRRL V274C ATMOS CB0]',
    '[DSRRL V274B DEFERRED CB0 CPU SHADOW]',
    '[DSRRL V274 ACTUAL LERP CB0]',
    '[DSRRL V272 INTERIOR LERP B12 ROUTE]',
    '[DSRRL V253 PTDE ENVSPEC RGBM]')){
    if(!$body.Contains($m)){throw "V274C compiled one-addon marker absent: $m"}
}
[IO.File]::WriteAllBytes($to,$bytes)
Write-Host "V274C_SHA256=$((Get-FileHash $to -Algorithm SHA256).Hash.ToLowerInvariant())"
Write-Host 'V274C_V272_STABLE_SHADER_UNMODIFIED=PASS'
Write-Host 'V274C_R19_STOCK_FOG_LS_BYTECODE_UNMODIFIED=PASS'
Write-Host 'V274C_ATMOS_CPU_SHADOW_CAPTURE_INTEGRATED=PASS'
Write-Host 'V274C_RUNTIME_CPU_SHADOW_AND_PIXEL=OPEN'
git diff --exit-code HEAD -- renderer-core/src renderer-core/include renderer-core/integrated renderer-core/tools/v258/build_v258_ptde_equipment_dormant_one_addon.ps1 renderer-core/tools/v274/build_v274_lerp_actual_cb85_witness.ps1
if($LASTEXITCODE -ne 0){throw 'V274C temporary source overlays not restored'}
