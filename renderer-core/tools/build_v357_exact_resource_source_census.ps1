param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot 'build_v350_gpu_resident_pointlight_off_rc2.ps1'
$script:base=[IO.File]::ReadAllText($source)
function Once($old,$new) {
 if (-not $script:base.Contains($old)) { throw "V353 drift: $old" }
 $script:base=$script:base.Replace($old,$new)
}
Once 'v350_rc2_gpu_resident_pointlight_off' 'v357_exact_resource_source_census'
Once 'build-v350-gpu-resident-pointlight-off-rc2' 'build-v357-resource-source-census'
Once 'out-v350-gpu-resident-pointlight-off-rc2' 'out-v357-resource-source-census'
Once 'DSRRL_V350_GPU_RESIDENT_POINTLIGHT_OFF_RC2.addon64' 'DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS.addon64'
Once '/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID' '/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID /DDSRRL_V296_ROOTCAUSE_TELEMETRY /DDSRRL_V351_TYPED_DSR_ONLY_OPERATOR /DDSRRL_V351_EXACT_MATERIAL_ROUTE_RECOVERY /DDSRRL_V352_LONG_SOAK_PRESENT /DDSRRL_V353_CPU_MODULE_PROFILER /DDSRRL_V354_EXACT_RESOURCE_RESNAPSHOT /DDSRRL_V356_EXACT_BACKING_SRV_ALIAS /DDSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS'
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
if ($LASTEXITCODE -ne 0) { throw 'V353 profiler contract failed' }
python renderer-core/tools/materialize_v354_exact_resource_resnapshot.py
if ($LASTEXITCODE -ne 0) { throw 'V354 exact resource patch failed' }
python renderer-core/tools/test_v354_exact_resource_resnapshot.py
if ($LASTEXITCODE -ne 0) { throw 'V354 test failed' }
python renderer-core/tools/materialize_v356_exact_backing_spec_rgb.py
if ($LASTEXITCODE -ne 0) { throw 'V356 source patch failed' }
python renderer-core/tools/materialize_v356_exact_probe_backing.py
if ($LASTEXITCODE -ne 0) { throw 'V356 probe patch failed' }
python renderer-core/tools/test_v356_exact_backing_spec_rgb.py
if ($LASTEXITCODE -ne 0) { throw 'V356 test failed' }
python renderer-core/tools/materialize_v357_exact_resource_source_census.py
if ($LASTEXITCODE -ne 0) { throw 'V357 negative source census materializer failed' }
python renderer-core/tools/test_v357_exact_resource_source_census.py
if ($LASTEXITCODE -ne 0) { throw 'V357 negative source census contract failed' }
'@
$temp=Join-Path $PSScriptRoot '_v353_generated_build.ps1'
try {
 [IO.File]::WriteAllText($temp,$script:base,[Text.UTF8Encoding]::new($false))
 & $temp -ReShadeInclude $ReShadeInclude
 if ($LASTEXITCODE -ne 0) { throw 'V353 build failed' }
 $file='out-v357-resource-source-census/DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS.addon64'
 $raw=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes((Resolve-Path $file)))
 foreach($m in @('[DSRRL V356 EXACT BACKING]','[DSRRL V356 PROBE BACKING]','[DSRRL V357 RESOURCE SOURCE]','[DSRRL V357 BC6H PRODUCER]','[DSRRL V354 RESOURCE RESNAPSHOT]','[DSRRL V354 SPEC REJECT]','[DSRRL V354 PROBE REJECT]','[DSRRL V353 CPU_STAGE]','[DSRRL V352 PERF]','[DSRRL V351 DSR_ONLY_OPERATOR]','v357_exact_resource_source_census')) {
   if (-not $raw.Contains($m)) { throw "Missing binary marker: $m" }
 }
 Write-Host 'V357 EXACT RESOURCE SOURCE CENSUS BUILD PASS; RUNTIME/PIXEL OPEN'
 Get-FileHash $file -Algorithm SHA256 | Format-List
} finally { Remove-Item $temp -Force -ErrorAction SilentlyContinue }
