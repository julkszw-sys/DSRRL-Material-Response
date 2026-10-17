param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot 'build_v350_gpu_resident_pointlight_off_rc2.ps1'
$script:base=[System.IO.File]::ReadAllText($source)
function Replace-Once([string]$old,[string]$new) {
    if (-not $script:base.Contains($old)) { throw "V352 source drift: missing $old" }
    $script:base=$script:base.Replace($old,$new)
}
Replace-Once 'v350_rc2_gpu_resident_pointlight_off' 'v352_full_telemetry_30s_soak'
Replace-Once 'build-v350-gpu-resident-pointlight-off-rc2' 'build-v352-long-soak'
Replace-Once 'out-v350-gpu-resident-pointlight-off-rc2' 'out-v352-long-soak'
Replace-Once 'DSRRL_V350_GPU_RESIDENT_POINTLIGHT_OFF_RC2.addon64' 'DSRRL_V352_LONG_SOAK_FULL_TELEMETRY.addon64'
Replace-Once '/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID' '/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID /DDSRRL_V296_ROOTCAUSE_TELEMETRY /DDSRRL_V351_TYPED_DSR_ONLY_OPERATOR /DDSRRL_V351_EXACT_MATERIAL_ROUTE_RECOVERY /DDSRRL_V352_LONG_SOAK_PRESENT'
Replace-Once '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF' '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=ON'
Replace-Once 'python renderer-core/tools/test_v304_typed_donor_contract.py' @'
python renderer-core/tools/test_v304_typed_donor_contract.py
if ($LASTEXITCODE -ne 0) { throw 'V304 provenance contract failed' }
python renderer-core/tools/materialize_v351_typed_lightbank_modified_renderer.py
if ($LASTEXITCODE -ne 0) { throw 'V351 modified renderer patch failed' }
python renderer-core/tools/materialize_v352_long_soak_telemetry.py
if ($LASTEXITCODE -ne 0) { throw 'V352 long soak materializer failed' }
python renderer-core/tools/test_v351_modified_renderer_source.py
if ($LASTEXITCODE -ne 0) { throw 'V351 modified renderer source contract failed' }
python renderer-core/tools/test_v352_long_soak_source.py
'@
$temp=Join-Path $PSScriptRoot '_v352_generated_build.ps1'
try {
    [System.IO.File]::WriteAllText($temp,$script:base,[System.Text.UTF8Encoding]::new($false))
    & $temp -ReShadeInclude $ReShadeInclude
    if ($LASTEXITCODE -ne 0) { throw 'V352 native build failed' }
    $file='out-v352-long-soak/DSRRL_V352_LONG_SOAK_FULL_TELEMETRY.addon64'
    $raw=[System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes((Resolve-Path $file)))
    if (-not $raw.Contains('[DSRRL V352 PERF]')) { throw 'V352 performance telemetry missing from binary' }
    if (-not $raw.Contains('v352_full_telemetry_30s_soak')) { throw 'V352 build flavor missing' }
    if (-not $raw.Contains('[DSRRL V351 DSR_ONLY_OPERATOR]')) { throw 'V351 modified renderer missing' }
    if (-not $raw.Contains('[DSRRL V296 ROOTCAUSE]')) { throw 'V296 ROOTCAUSE missing' }
    Write-Host 'V352 LONG SOAK CONSTRUCTION PASS; ALWAYS-ON RUNTIME/EFFECT COUNTERS, 30-SECOND PRESENT/QPC; NATIVE VERIFY OFF'
    Get-FileHash $file -Algorithm SHA256 | Format-List
} finally {
    Remove-Item $temp -Force -ErrorAction SilentlyContinue
}
