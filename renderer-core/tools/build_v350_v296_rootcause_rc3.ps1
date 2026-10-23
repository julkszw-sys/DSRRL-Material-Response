param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
$source = Join-Path $PSScriptRoot 'build_v350_gpu_resident_pointlight_off_rc2.ps1'
$base = [System.IO.File]::ReadAllText($source)
function Replace-Unique([string]$old,[string]$new) {
    if (-not $script:base.Contains($old)) { throw "Missing RC2 build contract: $old" }
    $script:base = $script:base.Replace($old,$new)
}
Replace-Unique 'v350_rc2_gpu_resident_pointlight_off' 'v350_rc3_v296_rootcause_pointlight_off'
Replace-Unique 'build-v350-gpu-resident-pointlight-off-rc2' 'build-v350-v296-rootcause-rc3'
Replace-Unique 'out-v350-gpu-resident-pointlight-off-rc2' 'out-v350-v296-rootcause-rc3'
Replace-Unique 'DSRRL_V350_GPU_RESIDENT_POINTLIGHT_OFF_RC2.addon64' 'DSRRL_V350_V296_ROOTCAUSE_RC3.addon64'
Replace-Unique '/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID' '/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID /DDSRRL_V296_ROOTCAUSE_TELEMETRY'
$script:base += "`nif (-not (Test-Path 'out-v350-v296-rootcause-rc3/DSRRL_V350_V296_ROOTCAUSE_RC3.addon64')) { throw 'RC3 artifact missing' }`n"
$temp = Join-Path $PSScriptRoot '_v350_rc3_diagnostic_generated_build.ps1'
try {
    [System.IO.File]::WriteAllText($temp,$base,[System.Text.UTF8Encoding]::new($false))
    & $temp -ReShadeInclude $ReShadeInclude
    if ($LASTEXITCODE -ne 0) { throw "RC3 native build failed" }
    $dest='out-v350-v296-rootcause-rc3/DSRRL_V350_V296_ROOTCAUSE_RC3.addon64'
    $raw=[System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes((Resolve-Path $dest)))
    if (-not $raw.Contains('[DSRRL V296 ROOTCAUSE]')) { throw 'RC3 rootcause witness missing in addon binary' }
    if (-not $raw.Contains('v350_rc3_v296_rootcause_pointlight_off')) { throw 'Wrong build flavor' }
    Write-Host 'RC3_CONSTRUCTION_PASS (game runtime/pixel OPEN)'
} finally {
    Remove-Item $temp -Force -ErrorAction SilentlyContinue
}
