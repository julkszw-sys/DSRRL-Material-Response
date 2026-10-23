param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
# Keep one build flag set: PTDE t10 post-EnvSpec and ONE registered Lerp
# shader for RX33/34/35, regardless of material route. Retain all other
# exact V338 materializer and runtime prerequisites.
$env:CL='/DDSRRL_V344_PTDE_LERP_SPEC_POST_ENV /DDSRRL_V345_UNIFIED_HEMENVLERP'
& ./renderer-core/tools/build_v338_nonmetal_lerp_one_addon.ps1 -ReShadeInclude $ReShadeInclude
if ($LASTEXITCODE -ne 0) { throw 'V345 single-shader build failed' }
New-Item -ItemType Directory -Force out-v345 | Out-Null
Copy-Item 'out-v338/DSRRL_V338_NONMETAL_LERP_YZW_C101.addon64' 'out-v345/DSRRL_V345_UNIFIED_PTDE_HEMENVLERP.addon64' -Force
if (-not (Test-Path 'out-v345/DSRRL_V345_UNIFIED_PTDE_HEMENVLERP.addon64')) {throw 'V345 addon absent'}
