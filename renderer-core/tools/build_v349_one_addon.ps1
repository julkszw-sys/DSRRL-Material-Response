param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference = 'Stop'
# Reuse exact V338->V347 one-addon lineage, add ONLY the two V349
# post-EnvSpec PTDE SpecRGB source consumer corrections.
$env:CL = '/DDSRRL_V344_PTDE_LERP_SPEC_POST_ENV /DDSRRL_V345_UNIFIED_HEMENVLERP /DDSRRL_V346_LERP_REJECT_STAGE /DDSRRL_V347_LERP_ENVDIFFUSE_REBASE /DDSRRL_V348_PTDE_LERP_SPECRGB_RGB_FIX'
& ./renderer-core/tools/build_v338_nonmetal_lerp_one_addon.ps1 -ReShadeInclude $ReShadeInclude
if ($LASTEXITCODE -ne 0) { throw 'V348 native compilation failed' }
New-Item -ItemType Directory -Force out-v349 | Out-Null
Copy-Item 'out-v338/DSRRL_V338_NONMETAL_LERP_YZW_C101.addon64' 'out-v348/DSRRL_V348_PTDE_LERP_RGB_SWIZZLE_FIXED.addon64' -Force
$f = 'out-v348/DSRRL_V348_PTDE_LERP_RGB_SWIZZLE_FIXED.addon64'
$b = [IO.File]::ReadAllBytes((Resolve-Path $f))
if ($b.Length -lt 500000) { throw 'V348 small binary' }
if ([Text.Encoding]::ASCII.GetString($b,0,2) -ne 'MZ') { throw 'V348 not PE' }
$off = [BitConverter]::ToInt32($b,0x3c)
if ([Text.Encoding]::ASCII.GetString($b,$off,4) -ne "PE`0`0") { throw 'V348 no PE signature' }
if ([BitConverter]::ToUInt16($b,$off+4) -ne 0x8664) { throw 'V348 not x64' }
$strings = [Text.Encoding]::ASCII.GetString($b)
if (-not $strings.Contains('[DSRRL V348 SPEC RGB]')) { throw 'V348 feature flag missing or inactive' }
$commit=(git rev-parse HEAD).Trim()
if (-not $strings.Contains($commit)) { throw 'V348 wrong source identity' }
Write-Host "V348 SHA256=$((Get-FileHash $f -Algorithm SHA256).Hash.ToLowerInvariant())"
