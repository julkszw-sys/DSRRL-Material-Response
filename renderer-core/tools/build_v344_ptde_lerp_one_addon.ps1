param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference = 'Stop'
$env:CL = '/DDSRRL_V344_PTDE_LERP_SPEC_POST_ENV'
& ./renderer-core/tools/build_v338_nonmetal_lerp_one_addon.ps1 -ReShadeInclude $ReShadeInclude
if ($LASTEXITCODE -ne 0) { throw 'V344 compilation failed' }
New-Item -Type Directory -Force out-v344 | Out-Null
Copy-Item 'out-v338/DSRRL_V338_NONMETAL_LERP_YZW_C101.addon64' 'out-v344/DSRRL_V344_PTDE_LERP_SPEC_POST_ENV.addon64' -Force
