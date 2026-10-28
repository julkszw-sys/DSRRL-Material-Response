param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
# Standalone diagnostic descendant. Never modify build_v230_production.ps1
# or frozen V359-RC1. One addon in distinct output directory.
$source=Join-Path $PSScriptRoot 'build_v230_production.ps1'
$body=[IO.File]::ReadAllText($source)
function Replace-One($old,$new) {
    if ($script:body.Split([string[]]@($old),[StringSplitOptions]::None).Count -ne 2) {
        throw "V413 diagnostic build drift: $old"
    }
    $script:body=$script:body.Replace($old,$new)
}
Replace-One "Once 'build-v350-gpu-resident-pointlight-off-rc2' 'build-v230-release'" "Once 'build-v350-gpu-resident-pointlight-off-rc2' 'build-v413-selector-diagnostic'"
Replace-One "Once 'out-v350-gpu-resident-pointlight-off-rc2' 'out-v230-release'" "Once 'out-v350-gpu-resident-pointlight-off-rc2' 'out-v413-selector-diagnostic'"
Replace-One "Once 'DSRRL_V350_GPU_RESIDENT_POINTLIGHT_OFF_RC2.addon64' 'DSRRL_Restored_Lighting_v2.3.0.addon64'" "Once 'DSRRL_V350_GPU_RESIDENT_POINTLIGHT_OFF_RC2.addon64' 'DSRRL_V413_Selector_Diagnostic.addon64'"
Replace-One "Once '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF' '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF'" "Once '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF' '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF -DDSRRL_EFFECT_TELEMETRY_DEFAULT_ON_ONLY=ON'"
Replace-One 'python renderer-core/tools/materialize_v230_release_cleanup.py' 'python renderer-core/tools/materialize_v413_selector_diagnostic.py'
Replace-One 'python renderer-core/tools/test_v230_release_contract.py' 'python renderer-core/tools/test_v413_diagnostic_build_contract.py'
Replace-One '$env:DSRRL_V230_LOG_FREE_AUDIT=\x271\x27' '# V413 diagnostic must preserve INFO source observers (no LOG_FREE audit)'
Replace-One "cmake --build build-v230-release --config Release --target dsrrl_renderer_core_integrated" "cmake --build build-v413-selector-diagnostic --config Release --target dsrrl_renderer_core_integrated"
Replace-One "$"+"file='out-v230-release/DSRRL_Restored_Lighting_v2.3.0.addon64'" "$"+"file='out-v413-selector-diagnostic/DSRRL_V413_Selector_Diagnostic.addon64'"
$tmp=Join-Path $PSScriptRoot '_v413_selector_diag_wrapper.ps1'
try {
    [IO.File]::WriteAllText($tmp,$script:body,[Text.UTF8Encoding]::new($false))
    & $tmp -ReShadeInclude $ReShadeInclude
    if($LASTEXITCODE -ne 0){throw 'V413 one-addon diagnostic build failed'}
} finally {
    Remove-Item $tmp -Force -ErrorAction SilentlyContinue
}
