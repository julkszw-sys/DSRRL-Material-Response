param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot 'build_v230_envdiffuse_rgba_fix.ps1'
$base=[IO.File]::ReadAllText($source)
function RequireReplace([string]$old,[string]$new,[string]$label,[int]$expected=1) {
    $hits=($script:base.Split(@($old),[StringSplitOptions]::None).Count - 1)
    if ($hits -ne $expected) { throw "Reflection builder anchor drift: $label hits=$hits expected=$expected" }
    $script:base=$script:base.Replace($old,$new)
}
# Keep original owner235 PR484 shader swizzle fix and all native CMake features.
RequireReplace '/DDSRRL_V359_ATTESTED_GI_NAME_ALIAS' '/DDSRRL_V359_ATTESTED_GI_NAME_ALIAS /DDSRRL_V230_REFLECTION_TO_342 /DDSRRL_V230_GI_REFLECTION_SOURCE_BRANCH' 'feature define'
$v359Verify=@'
python renderer-core/tools/materialize_v230_reflection_342_name_alias.py
if ($LASTEXITCODE -ne 0) { throw 'V230 exact GI Reflection source alias materialization failed' }
python renderer-core/tools/test_v230_reflection_342_alias.py
if ($LASTEXITCODE -ne 0) { throw 'V230 exact Reflection semantic bridge contract failed' }
python renderer-core/tools/test_v359_attested_gi_name_alias.py
python renderer-core/tools/materialize_v230_gi_origin_probe_rc2.py
if ($LASTEXITCODE -ne 0) { throw 'PR486 probe source rejected' }
python renderer-core/tools/test_v230_gi_origin_probe_rc2.py
if ($LASTEXITCODE -ne 0) { throw 'PR486 source contract rejected' }
'@
RequireReplace "python renderer-core/tools/test_v359_attested_gi_name_alias.py" $v359Verify 'post-V359 exact source-name origin'
RequireReplace 'DSRRL_Restored_Lighting_v2.3.0_PTDE_EnvDiffuse_RGBA_FIXED.addon64' 'DSRRL_v230_PTDE_EnvDiffuse_RGBA_FIXED_GI_ORIGIN_RC2.addon64' 'release artifact name' 2
# Parent script is still a nested, auditable build and does all its original
# release-contract and PE tests. Do not stage incomplete sidecars.
$temp=Join-Path $PSScriptRoot '_v230_reflection_342_generated_build.ps1'
try {
 [IO.File]::WriteAllText($temp,$script:base,[Text.UTF8Encoding]::new($false))
 & $temp -ReShadeInclude $ReShadeInclude
 if ($LASTEXITCODE -ne 0) { throw 'GI Reflection v230 native build failed' }
 $dest='out-v230-release/DSRRL_v230_PTDE_EnvDiffuse_RGBA_FIXED_GI_ORIGIN_RC2.addon64'
 if (!(Test-Path $dest)) { throw 'Missing exact one-addon release binary' }
 $data=[IO.File]::ReadAllBytes((Resolve-Path $dest))
 $ascii=[Text.Encoding]::ASCII.GetString($data)
 if (!$ascii.Contains('v230_production') -or !$ascii.Contains('2.3.0')) { throw 'Missing owner235 identity' }
 # MSVC may lower short UTF-16 literals into integer immediates; do not
 # falsely fail on absent contiguous source-name string.
 if (!$ascii.Contains('[DSRRL V230 GI REFLECTION NAME CANDIDATE]')) { throw 'Reflection source-name branch not compiled' }
 Get-FileHash $dest -Algorithm SHA256 | Format-List
 Write-Host 'CONSTRUCTION=PASS; COMPATIBILITY=PASS; reflection 15 runtime mapping OPEN; PIXEL=OPEN'
} finally { Remove-Item $temp -Force -ErrorAction SilentlyContinue }
