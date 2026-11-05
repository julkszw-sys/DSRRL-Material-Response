param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
# V438-only: MSVC C4127 is a known baseline constexpr/while warning; keep /WX
# for every other diagnostic and compiler warning.
$env:CL = (($env:CL + ' /wd4127').Trim())
# Build V438 as a single integrated addon, with V437 ancestry and protected R43 intact.
$source=Join-Path $PSScriptRoot 'build_v230_production.ps1'
$body=[IO.File]::ReadAllText($source)
# V350 nested script resets $env:CL. Scope diagnostic C4127 workaround to its generated copy.
$nested = '$script:base=[IO.File]::ReadAllText($source)'
$nestedPatch = @'
$script:base=[IO.File]::ReadAllText($source)
if (-not $script:base.Contains('$env:CL = ''/DDSRRL')) { throw 'V438 MSVC C4127 scope drift' }
$script:base=$script:base.Replace('$env:CL = ''/DDSRRL', '$env:CL = ''/wd4127 /DDSRRL')
'@
if ($body.Split([string[]]@($nested),[StringSplitOptions]::None).Count -ne 2) { throw 'V438 nested builder preimage drift' }
$body=$body.Replace($nested,$nestedPatch.TrimEnd())
function Replace-One([string]$old,[string]$new) {
 if($script:body.Split([string[]]@($old),[StringSplitOptions]::None).Count -ne 2) {
    throw "V438 baseline drift: $old"
 }
 $script:body=$script:body.Replace($old,$new)
}
Replace-One "Once 'build-v350-gpu-resident-pointlight-off-rc2' 'build-v230-release'" "Once 'build-v350-gpu-resident-pointlight-off-rc2' 'build-v438-one-shot-diagnostic'"
Replace-One "Once 'out-v350-gpu-resident-pointlight-off-rc2' 'out-v230-release'" "Once 'out-v350-gpu-resident-pointlight-off-rc2' 'out-v438-one-shot-diagnostic'"
Replace-One "Once 'DSRRL_V350_GPU_RESIDENT_POINTLIGHT_OFF_RC2.addon64' 'DSRRL_Restored_Lighting_v2.3.0.addon64'" "Once 'DSRRL_V350_GPU_RESIDENT_POINTLIGHT_OFF_RC2.addon64' 'DSRRL_V438_Armor_Flicker_OneShot.addon64'"
Replace-One "Once '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF' '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF'" "Once '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF' '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF -DDSRRL_EFFECT_TELEMETRY_DEFAULT_ON_ONLY=ON'"
Replace-One " /DDSRRL_V359_ATTESTED_GI_NAME_ALIAS'" " /DDSRRL_V359_ATTESTED_GI_NAME_ALIAS /DDSRRL_V415_FLVER_MODEL_ID_DIAG /DDSRRL_V416_FGMODEL_PARENT_DIAG /DDSRRL_V417_FGMODEL_REGISTRY_DIAG /DDSRRL_V418_RX34_GENERATIONAL_ISSUED_JOIN /DDSRRL_V419_RX34_PREDRAW_NATIVE_STATE /DDSRRL_V438_ONE_SHOT_CAPTURE'"
Replace-One 'python renderer-core/tools/materialize_v230_release_cleanup.py' 'python renderer-core/tools/materialize_v438_one_shot_diagnostic.py'
Replace-One 'python renderer-core/tools/test_v230_release_contract.py' 'python renderer-core/tools/test_v438_one_shot_contract.py'
Replace-One '$env:DSRRL_V230_LOG_FREE_AUDIT=''1''' '# V438 keeps all INFO and WARN telemetry, no LOG_FREE audit'
Replace-One "Once 'cmake --build build-v230-release --config Release --target dsrrl_renderer_core_integrated'" "Once 'cmake --build build-v438-one-shot-diagnostic --config Release --target dsrrl_renderer_core_integrated'"
Replace-One "cmake --build build-v230-release --config Release --target dsrrl_renderer_core_integrated" "cmake --build build-v438-one-shot-diagnostic --config Release --target dsrrl_renderer_core_integrated"
Replace-One '$file=''out-v230-release/DSRRL_Restored_Lighting_v2.3.0.addon64''' '$file=''out-v438-one-shot-diagnostic/DSRRL_V438_Armor_Flicker_OneShot.addon64'''
$v388Exit='if ($LASTEXITCODE -ne 0) { throw ''V388 STABLE source to indexed draw join contract failed'' }'
Replace-One $v388Exit ($v388Exit + "`npython renderer-core/tools/v438_apply_draw_part_b.py`nif (`$LASTEXITCODE -ne 0) { throw 'V438 late source overlay failed' }")
$tmp=Join-Path $PSScriptRoot '_v438_one_shot_generated_build.ps1'
try {
 [IO.File]::WriteAllText($tmp,$script:body,[Text.UTF8Encoding]::new($false))
 & $tmp -ReShadeInclude $ReShadeInclude
 if($LASTEXITCODE -ne 0){throw 'V438 single addon diagnostic build failed'}
 $addon='out-v438-one-shot-diagnostic/DSRRL_V438_Armor_Flicker_OneShot.addon64'
 if(!(Test-Path $addon)){throw 'V438 addon missing'}
 $raw=[IO.File]::ReadAllBytes((Resolve-Path $addon))
 if($raw.Length -lt 1000000 -or $raw[0] -ne 0x4d -or $raw[1] -ne 0x5a){throw 'V438 invalid PE addon'}
 if((Get-ChildItem 'out-v438-one-shot-diagnostic' -Filter '*.addon64').Count -ne 1){throw 'More than one addon in output'}
 $text=[Text.Encoding]::ASCII.GetString($raw)
 foreach($marker in @('[DSRRL V438] WRAPPER_HOOKS_ACTIVE','[DSRRL V438] CAPTURE_START','[DSRRL V419 RX34 PRE-DRAW NATIVE STATE]','[DSRRL V418 RX34 MODEL ISSUED JOIN]')) {
    if(!$text.Contains($marker)){throw "V438 missing binary marker: $marker"}
 }
 Get-FileHash $addon -Algorithm SHA256 | Format-List
 Write-Host 'V438 CONSTRUCTION/COMPATIBILITY: Windows local build PASS; gameplay runtime remains OPEN'
} finally {Remove-Item $tmp -Force -ErrorAction SilentlyContinue}
