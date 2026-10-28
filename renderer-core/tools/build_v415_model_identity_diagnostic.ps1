param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
# Standalone diagnostic descendant. Never modify build_v230_production.ps1
# or frozen V359-RC1. One addon in distinct output directory.
$source=Join-Path $PSScriptRoot 'build_v230_production.ps1'
$body=[IO.File]::ReadAllText($source)
function Replace-One($old,$new) {
    if ($script:body.Split([string[]]@($old),[StringSplitOptions]::None).Count -ne 2) {
        throw "V415 diagnostic build drift: $old"
    }
    $script:body=$script:body.Replace($old,$new)
}
Replace-One "Once 'build-v350-gpu-resident-pointlight-off-rc2' 'build-v230-release'" "Once 'build-v350-gpu-resident-pointlight-off-rc2' 'build-v415-model-identity-diagnostic'"
Replace-One "Once 'out-v350-gpu-resident-pointlight-off-rc2' 'out-v230-release'" "Once 'out-v350-gpu-resident-pointlight-off-rc2' 'out-v415-model-identity-diagnostic'"
Replace-One "Once 'DSRRL_V350_GPU_RESIDENT_POINTLIGHT_OFF_RC2.addon64' 'DSRRL_Restored_Lighting_v2.3.0.addon64'" "Once 'DSRRL_V350_GPU_RESIDENT_POINTLIGHT_OFF_RC2.addon64' 'DSRRL_V415_Model_Identity_Diagnostic.addon64'"
Replace-One "Once '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF' '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF'" "Once '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF' '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF -DDSRRL_EFFECT_TELEMETRY_DEFAULT_ON_ONLY=ON'"
# V415 compile-time guard: emit observations only in this dedicated build.
Replace-One " /DDSRRL_V359_ATTESTED_GI_NAME_ALIAS'" " /DDSRRL_V359_ATTESTED_GI_NAME_ALIAS /DDSRRL_V415_FLVER_MODEL_ID_DIAG'"
Replace-One 'python renderer-core/tools/materialize_v230_release_cleanup.py' 'python renderer-core/tools/materialize_v415_model_identity_diagnostic.py'
Replace-One 'python renderer-core/tools/test_v230_release_contract.py' 'python renderer-core/tools/test_v415_diagnostic_build_contract.py'
Replace-One '$env:DSRRL_V230_LOG_FREE_AUDIT=''1''' '# V415 diagnostic must preserve INFO source observers (no LOG_FREE audit)'
Replace-One "Once 'cmake --build build-v230-release --config Release --target dsrrl_renderer_core_integrated'" "Once 'cmake --build build-v415-model-identity-diagnostic --config Release --target dsrrl_renderer_core_integrated'"
Replace-One "cmake --build build-v230-release --config Release --target dsrrl_renderer_core_integrated" "cmake --build build-v415-model-identity-diagnostic --config Release --target dsrrl_renderer_core_integrated"
Replace-One '$file=''out-v230-release/DSRRL_Restored_Lighting_v2.3.0.addon64''' '$file=''out-v415-model-identity-diagnostic/DSRRL_V415_Model_Identity_Diagnostic.addon64'''
$tmp=Join-Path $PSScriptRoot '_v415_model_identity_diag_wrapper.ps1'
try {
    [IO.File]::WriteAllText($tmp,$script:body,[Text.UTF8Encoding]::new($false))
    & $tmp -ReShadeInclude $ReShadeInclude
    if($LASTEXITCODE -ne 0){throw 'V415 one-addon diagnostic build failed'}
} finally {
    Remove-Item $tmp -Force -ErrorAction SilentlyContinue
}
