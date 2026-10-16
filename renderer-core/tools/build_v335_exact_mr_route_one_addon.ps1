param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
$ReShadeInclude = (Resolve-Path $ReShadeInclude).Path
python renderer-core/tools/apply_v335_exact_mr_route.py --apply
if ($LASTEXITCODE -ne 0) { throw 'V335 patch rejected' }
$env:CL='/DDSRRL_V335_MR_EXACT_OWNER_ROUTE'
& "$PSScriptRoot/build_v326_nonmetal_lerp_t10_identity_one_addon.ps1" -ReShadeInclude $ReShadeInclude
if ($LASTEXITCODE -ne 0) { throw 'native build failed' }
python renderer-core/tools/test_v335_mr_exact_owner_route.py
if ($LASTEXITCODE -ne 0) { throw 'V335 contract failure' }
New-Item -ItemType Directory -Force out-v335 | Out-Null
Copy-Item out-v326/DSRRL_V326_NONMETAL_LERP_T10_RGB_IDENTITY.addon64 out-v335/DSRRL_V335_MR_EXACT_OWNER_ROUTE.addon64 -Force
Get-FileHash out-v335/DSRRL_V335_MR_EXACT_OWNER_ROUTE.addon64 -Algorithm SHA256
