param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot 'build_v350_gpu_resident_pointlight_off_rc2.ps1'
$script:base=[IO.File]::ReadAllText($source)
function Once($old,$new) {
 if (-not $script:base.Contains($old)) { throw "V353 drift: $old" }
 $script:base=$script:base.Replace($old,$new)
}
Once 'v350_rc2_gpu_resident_pointlight_off' 'v353_cpu_modules_full_soak'
Once 'build-v350-gpu-resident-pointlight-off-rc2' 'build-v353-modules'
Once 'out-v350-gpu-resident-pointlight-off-rc2' 'out-v353-modules'
Once 'DSRRL_V350_GPU_RESIDENT_POINTLIGHT_OFF_RC2.addon64' 'DSRRL_V353_CPU_MODULE_PROFILER.addon64'
Once '/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID' '/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID /DDSRRL_V296_ROOTCAUSE_TELEMETRY /DDSRRL_V351_TYPED_DSR_ONLY_OPERATOR /DDSRRL_V351_EXACT_MATERIAL_ROUTE_RECOVERY /DDSRRL_V352_LONG_SOAK_PRESENT /DDSRRL_V353_CPU_MODULE_PROFILER'
Once '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF' '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=ON'
Once 'python renderer-core/tools/test_v304_typed_donor_contract.py' @'
python renderer-core/tools/test_v304_typed_donor_contract.py
if ($LASTEXITCODE -ne 0) { throw 'V304 contract failed' }
python renderer-core/tools/materialize_v351_typed_lightbank_modified_renderer.py
if ($LASTEXITCODE -ne 0) { throw 'V351 patch failed' }
python renderer-core/tools/materialize_v352_long_soak_telemetry.py
if ($LASTEXITCODE -ne 0) { throw 'V352 patch failed' }
python renderer-core/tools/materialize_v353_sampled_module_profiler.py
if ($LASTEXITCODE -ne 0) { throw 'V353 patch failed' }
python renderer-core/tools/test_v351_modified_renderer_source.py
if ($LASTEXITCODE -ne 0) { throw 'V351 contract failed' }
python renderer-core/tools/test_v352_long_soak_source.py
if ($LASTEXITCODE -ne 0) { throw 'V352 contract failed' }
python renderer-core/tools/test_v353_sampled_module_profiler.py
'@
$temp=Join-Path $PSScriptRoot '_v353_generated_build.ps1'
try {
 [IO.File]::WriteAllText($temp,$script:base,[Text.UTF8Encoding]::new($false))
 & $temp -ReShadeInclude $ReShadeInclude
 if ($LASTEXITCODE -ne 0) { throw 'V353 build failed' }
 $file='out-v353-modules/DSRRL_V353_CPU_MODULE_PROFILER.addon64'
 $raw=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes((Resolve-Path $file)))
 foreach($m in @('[DSRRL V353 CPU_STAGE]','[DSRRL V352 PERF]','[DSRRL V351 DSR_ONLY_OPERATOR]','v353_cpu_modules_full_soak')) {
   if (-not $raw.Contains($m)) { throw "Missing binary marker: $m" }
 }
 Write-Host 'V353 CPU PROFILER BUILD PASS; RUNTIME/PIXEL OPEN'
 Get-FileHash $file -Algorithm SHA256 | Format-List
} finally { Remove-Item $temp -Force -ErrorAction SilentlyContinue }
