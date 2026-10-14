# V312 private owner WARP attestation. Driver compatibility only, no game/runtime/pixel claim.
param(
 [Parameter(Mandatory=$true)][string]$OwnerDirectory,
 [Parameter(Mandatory=$true)][string]$WarpGateExe,
 [Parameter(Mandatory=$true)][string]$AttestationOutput
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if(Test-Path -LiteralPath $AttestationOutput){throw 'Never overwrite an existing attestation'}
$receipt=Get-Content -LiteralPath (Join-Path $OwnerDirectory 'V300_OWNER_LOCAL_RECEIPT.json') -Raw | ConvertFrom-Json
if($receipt.schema -ne 'V300_OWNER_LOCAL_SHADER_TRIPLET_VERIFIED'){throw 'Owner source receipt invalid'}
& $WarpGateExe --self-test
if($LASTEXITCODE -ne 0){throw 'WARP fixture failed'}
$stages=@(
 @('stable_r17','final_r17_RX',64),
 @('lerp_r19','final_r19_RX',96),
 @('v286','v286_R5_R11_R7_LerpRX',96),
 @('v297_postls','v286_R5_R11_R7_LerpRX',96),
 @('v301/HemEnvLerp','v297_RX',96)
)
$files=@()
foreach($stage in $stages){
 foreach($rx in @(33,34,35)){
  $relative=('{0}/{1}{2}.dxbc' -f $stage[0],$stage[1],$rx)
  $path=Join-Path $OwnerDirectory ($relative -replace '/', [IO.Path]::DirectorySeparatorChar)
  if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing owner DXBC $relative"}
  $sha=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
  & $WarpGateExe $path ([string]$stage[2]) ('RX'+$rx)
  if($LASTEXITCODE -ne 0){throw "Real owner WARP CreatePixelShader rejected $relative"}
  $files+=@{relative_path=$relative;sha256=$sha;b12_declared_bytes=[int]$stage[2];warp_d3dreflect='PASS';warp_create_pixel_shader='PASS'}
 }
}
if(@($files).Count -ne 15){throw 'Incomplete owner WARP run'}
$report=@{schema='DSRRL_V312_OWNER_WINDOWS_WARP_V1';result='ALL_15_REAL_DXBC_WARP_PASS';recorded_utc=[DateTime]::UtcNow.ToString('o');pixel='OPEN';runtime='OPEN';files=$files}
$temp=$AttestationOutput+'.partial'
[IO.File]::WriteAllText($temp,($report | ConvertTo-Json -Depth 8),[System.Text.UTF8Encoding]::new($false))
Move-Item -LiteralPath $temp -Destination $AttestationOutput
Write-Output 'V312_OWNER_15_REAL_SHADER_WARP_PASS (driver compatibility only)'
