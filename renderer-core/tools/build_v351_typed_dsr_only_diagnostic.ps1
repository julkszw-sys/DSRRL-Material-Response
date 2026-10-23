param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot 'build_v350_gpu_resident_pointlight_off_rc2.ps1'
$script:base=[System.IO.File]::ReadAllText($source)
function Replace-Once([string]$old,[string]$new) {
    if (-not $script:base.Contains($old)) { throw "V351 source drift: missing $old" }
    $script:base=$script:base.Replace($old,$new)
}
Replace-Once 'v350_rc2_gpu_resident_pointlight_off' 'v351_typed_dsr_only_modified_renderer'
Replace-Once 'build-v350-gpu-resident-pointlight-off-rc2' 'build-v351-typed-dsr-only'
Replace-Once 'out-v350-gpu-resident-pointlight-off-rc2' 'out-v351-typed-dsr-only'
Replace-Once 'DSRRL_V350_GPU_RESIDENT_POINTLIGHT_OFF_RC2.addon64' 'DSRRL_V351_TYPED_DSR_ONLY_DIAGNOSTIC.addon64'
Replace-Once '/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID' '/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID /DDSRRL_V296_ROOTCAUSE_TELEMETRY /DDSRRL_V351_TYPED_DSR_ONLY_OPERATOR /DDSRRL_V351_EXACT_MATERIAL_ROUTE_RECOVERY'
Replace-Once 'python renderer-core/tools/test_v304_typed_donor_contract.py' @'
python renderer-core/tools/test_v304_typed_donor_contract.py
if ($LASTEXITCODE -ne 0) { throw 'V304 provenance contract failed' }
python renderer-core/tools/materialize_v351_typed_lightbank_modified_renderer.py
if ($LASTEXITCODE -ne 0) { throw 'V351 typed modified renderer patch failed' }
python renderer-core/tools/test_v351_modified_renderer_source.py
'@
$temp=Join-Path $PSScriptRoot '_v351_generated_build.ps1'
try {
    [System.IO.File]::WriteAllText($temp,$script:base,[System.Text.UTF8Encoding]::new($false))
    & $temp -ReShadeInclude $ReShadeInclude
    if ($LASTEXITCODE -ne 0) { throw 'V351 native build failed' }
    $file='out-v351-typed-dsr-only/DSRRL_V351_TYPED_DSR_ONLY_DIAGNOSTIC.addon64'
    $raw=[System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes((Resolve-Path $file)))
    if (-not $raw.Contains('[DSRRL V351 DSR_ONLY_OPERATOR]')) { throw 'V351 modified consumer marker absent' }
    if (-not $raw.Contains('v351_typed_dsr_only_modified_renderer')) { throw 'V351 build flavor absent' }
    if (-not $raw.Contains('[DSRRL V296 ROOTCAUSE]')) { throw 'V296 regression witness absent' }
    Write-Host 'V351 CONSTRUCTION PASS; RELEASE BLOCKED UNTIL LIVE ROW64 AND ALL MATERIAL ROUTES PIXEL TESTED'
    Get-FileHash $file -Algorithm SHA256 | Format-List
} finally {
    Remove-Item $temp -Force -ErrorAction SilentlyContinue
}
