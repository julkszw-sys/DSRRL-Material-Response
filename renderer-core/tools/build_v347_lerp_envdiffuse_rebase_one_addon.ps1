param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
# Keep one build flag set: PTDE t10 post-EnvSpec and ONE registered Lerp
# shader for RX33/34/35, regardless of material route. Retain all other
# exact V338 materializer and runtime prerequisites.
$env:CL='/DDSRRL_V344_PTDE_LERP_SPEC_POST_ENV /DDSRRL_V345_UNIFIED_HEMENVLERP /DDSRRL_V346_LERP_REJECT_STAGE /DDSRRL_V347_LERP_ENVDIFFUSE_REBASE'
& ./renderer-core/tools/build_v338_nonmetal_lerp_one_addon.ps1 -ReShadeInclude $ReShadeInclude
if ($LASTEXITCODE -ne 0) { throw 'V347 R19 relocation corrected build failed' }
New-Item -ItemType Directory -Force out-v347 | Out-Null
Copy-Item 'out-v338/DSRRL_V338_NONMETAL_LERP_YZW_C101.addon64' 'out-v347/DSRRL_V347_LERP_ENVDIFFUSE_REBASE.addon64' -Force
if (-not (Test-Path 'out-v347/DSRRL_V347_LERP_ENVDIFFUSE_REBASE.addon64')) {throw 'V347 addon absent'}
