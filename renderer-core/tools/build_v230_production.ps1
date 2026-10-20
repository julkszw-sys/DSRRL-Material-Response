param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot 'build_v350_gpu_resident_pointlight_off_rc2.ps1'
$script:base=[IO.File]::ReadAllText($source)
function Once($old,$new) {
 if (-not $script:base.Contains($old)) { throw "V353 drift: $old" }
 $script:base=$script:base.Replace($old,$new)
}
Once 'v350_rc2_gpu_resident_pointlight_off' 'v230_production'
Once 'build-v350-gpu-resident-pointlight-off-rc2' 'build-v230-release'
Once 'out-v350-gpu-resident-pointlight-off-rc2' 'out-v230-release'
Once 'DSRRL_V350_GPU_RESIDENT_POINTLIGHT_OFF_RC2.addon64' 'DSRRL_Restored_Lighting_v2.3.0.addon64'
Once '/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID' '/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID /DDSRRL_V230_PRODUCTION /DDSRRL_V351_TYPED_DSR_ONLY_OPERATOR /DDSRRL_V351_EXACT_MATERIAL_ROUTE_RECOVERY /DDSRRL_V354_EXACT_RESOURCE_RESNAPSHOT /DDSRRL_V356_EXACT_BACKING_SRV_ALIAS /DDSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS /DDSRRL_V358_EXACT_PROBE_ORIGIN /DDSRRL_V359_ATTESTED_GI_NAME_ALIAS /DDSRRL_V230_STOCK_DSR_ENVDIFFUSE /DDSRRL_V230_FAILOPEN_DIAGNOSTICS'
Once '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF' '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF'
Once 'cmake --build build-v230-release --config Release --target dsrrl_renderer_core_integrated' @'
python renderer-core/tools/audit_active_islands_hotpath.py --source-dir renderer-core
if ($LASTEXITCODE -ne 0) { throw 'V350/V359 original hot-path/source audit failed BEFORE stripping diagnostics' }
python renderer-core/tools/materialize_v230_release_cleanup.py
if ($LASTEXITCODE -ne 0) { throw 'v2.3.0 cleanup failed' }
python renderer-core/tools/test_v230_release_contract.py
if ($LASTEXITCODE -ne 0) { throw 'v2.3.0 release contract failed' }
python renderer-core/tools/test_v230_stock_envdiffuse_contract.py
if ($LASTEXITCODE -ne 0) { throw 'Stock DSR EnvDiffuse isolation contract failed' }
$env:DSRRL_V230_LOG_FREE_AUDIT='1'
cmake --build build-v230-release --config Release --target dsrrl_renderer_core_integrated
'@

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
python renderer-core/tools/materialize_v358_exact_lifetime_probe_origin.py
if ($LASTEXITCODE -ne 0) { throw 'V358 source materializer failed' }
python renderer-core/tools/test_v358_exact_lifetime_probe_origin.py
if ($LASTEXITCODE -ne 0) { throw 'V358 source contract failed' }
python renderer-core/tools/materialize_v359_attested_gi_name_alias.py
if ($LASTEXITCODE -ne 0) { throw 'V359 materializer failed' }
python renderer-core/tools/materialize_v359_view_scope_identity.py
if ($LASTEXITCODE -ne 0) { throw 'V359 exact view source recovery failed' }
python renderer-core/tools/test_v359_attested_gi_name_alias.py
if ($LASTEXITCODE -ne 0) { throw 'V359 test failed' }

'@
$temp=Join-Path $PSScriptRoot '_v230_generated_build.ps1'
try {
 [IO.File]::WriteAllText($temp,$script:base,[Text.UTF8Encoding]::new($false))
 & $temp -ReShadeInclude $ReShadeInclude
 if ($LASTEXITCODE -ne 0) { throw 'v2.3.0 native build failed' }
 $file='out-v230-release/DSRRL_Restored_Lighting_v2.3.0.addon64'
 $raw=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes((Resolve-Path $file)))
 foreach($m in @('[DSRRL V350 PTDE LERP RGB]','[DSRRL GPU RESIDENT RC1]','v230_production','2.3.0','[DSRRL V230 STOCK DSR ENVDIFFUSE]')) {
   if (-not $raw.Contains($m)) { throw "Missing binary marker: $m" }
 }
 Write-Host 'v2.3.0 STOCK DSR ENVDIFFUSE diagnostic native build PASS; runtime and pixel OPEN'
 Get-FileHash $file -Algorithm SHA256 | Format-List
} finally { Remove-Item $temp -Force -ErrorAction SilentlyContinue }
