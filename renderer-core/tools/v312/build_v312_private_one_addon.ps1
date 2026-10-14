# V312 owner-private one-addon build; only after exact 15 owner shader WARP PASS.
# Builds in detached throwaway Git worktree. Does not modify DSR EXE or R43 RC1.
param(
 [Parameter(Mandatory=$true)][string]$Repository,
 [Parameter(Mandatory=$true)][string]$AuthorizedStage,
 [Parameter(Mandatory=$true)][string]$ReshadeIncludeDir,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$SourceRef='agent/v312-owner-gate-20261014'
)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
if(Test-Path -LiteralPath $OutputDirectory){throw 'No overwrite of prior outputs'}
$status=Get-Content -LiteralPath (Join-Path $AuthorizedStage 'V312_ADMISSION.json') -Raw | ConvertFrom-Json
if($status.phase -ne 'DIAGNOSTIC_ADMISSION' -and $status.state -ne 'DIAGNOSTIC_ADMISSION'){throw 'No WARP approval; refuse active addon'}
if($status.windows_warp -ne 'PASS' -and $status.windows_warp_owner -ne 'PASS'){throw 'No all-15 WARP gate'}
$hdr=Join-Path $AuthorizedStage 'include/hemenvlerp_v301_owner_approval.hpp'
if(!(Test-Path -LiteralPath $hdr -PathType Leaf)){throw 'Missing private RX approval'}
foreach($rx in @(33,34,35)){
 $file=Join-Path $AuthorizedStage ("DSR/DSRRL/EnvSpec/HemEnvLerp/v297_RX$rx.dxbc")
 if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing approved sidecar RX$rx"}
}
$sha=(& git -C $Repository rev-parse $SourceRef).Trim()
if($LASTEXITCODE -ne 0 -or $sha.Length -ne 40){throw 'Invalid source ref'}
$work=Join-Path (Split-Path -Parent $OutputDirectory) ('.v312-worktree-'+[Guid]::NewGuid().ToString('N'))
& git -C $Repository worktree add --detach $work $sha
if($LASTEXITCODE -ne 0){throw 'Cannot create detached worktree'}
try{
 $include=Join-Path $work 'renderer-core/include/dsrrl/runtime/hemenvlerp_v301_owner_approval.hpp'
 if(!(Test-Path -LiteralPath $include -PathType Leaf)){throw 'V301 loader unavailable'}
 Copy-Item -LiteralPath $hdr -Destination $include -Force
 Push-Location $work
 try{
  $env:GITHUB_SHA=$sha
  & renderer-core/tools/v287/configure.ps1 -BuildDirectory 'build-v312-private'
  if($LASTEXITCODE -ne 0){throw 'V287 base configure failed'}
  & cmake -S renderer-core/integrated -B build-v312-private "-DRESHADE_INCLUDE_DIR=$ReshadeIncludeDir" -DDSRRL_PMETAL_STABLE_LS_PTDE_POSTDOMAIN_STOCK_FOG=ON -DDSRRL_PMETAL_V289_UNPAIRED_LS_DIAGNOSTIC_ONLY=ON -DDSRRL_PMETAL_V301_OWNER_DXBC_DIAGNOSTIC=ON
  if($LASTEXITCODE -ne 0){throw 'V301 guarded configure failed'}
  & cmake --build build-v312-private --config Release --target dsrrl_renderer_core_integrated
  if($LASTEXITCODE -ne 0){throw 'Native build failed'}
  $mods=@(Get-ChildItem -LiteralPath build-v312-private -Recurse -File -Filter '*.addon64')
  if($mods.Count -ne 1){throw 'Expected exactly one addon'}
  New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
  $target=Join-Path $OutputDirectory 'DSRRL_V312_PRIVATE_DIAGNOSTIC.addon64'
  Copy-Item -LiteralPath $mods[0].FullName -Destination $target
  @{schema='DSRRL_V312_PRIVATE_ONE_ADDON_V1';source_commit=$sha;addon_sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant();construction='PASS';warp_owner='PASS';runtime='OPEN';pixel='OPEN';fog='STOCK_DSR';latest_hook_source='PROTECTED'} |
    ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'V312_BUILD_RECEIPT.json')
 } finally { Pop-Location }
} finally {
 & git -C $Repository worktree remove --force $work
}
Write-Output 'V312_SINGLE_ADDON_CONSTRUCTION_PASS; RUNTIME and PIXEL OPEN'
