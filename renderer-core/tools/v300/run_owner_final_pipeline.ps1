param(
 [Parameter(Mandatory=$true)][string]$Part02Zip,
 [Parameter(Mandatory=$true)][string]$NewOutputDirectory,
 [string]$RepoRoot=".",
 [string]$BinDirectory="."
)
$ErrorActionPreference="Stop"
$repo=(Resolve-Path $RepoRoot).Path
$bin=(Resolve-Path $BinDirectory).Path
$target=[IO.Path]::GetFullPath($NewOutputDirectory)
if(Test-Path $target){throw "V300 refuses existing output"}
$parent=Split-Path $target -Parent
if(!(Test-Path $parent)){throw "V300 output parent missing"}
$staging=Join-Path $parent (".v300_owner_"+[guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory $staging | Out-Null
function Checked([string]$exe,[string[]]$params){
 & $exe @params
 if($LASTEXITCODE -ne 0){throw "V300 child failed: $exe"}
}
try{
 $original=Join-Path $staging "original"
 Checked "python" @("$repo/renderer-core/tools/v300/extract_six_original_owner_rx.py",$Part02Zip,$original)
 $stable=Join-Path $staging "stable_r17"
 $lerp=Join-Path $staging "lerp_r19"
 Checked "$bin/v300-final-r17-exporter.exe" @("$original/stable33.dxbc","$original/stable34.dxbc","$original/stable35.dxbc",$stable)
 Checked "$bin/v276-final-r19-exporter.exe" @("$original/lerp33.dxbc","$original/lerp34.dxbc","$original/lerp35.dxbc",$lerp)
 $pins=@(
  @("stable_r17/final_r17_RX33.dxbc","b8a9f32010dd279ffae5dd3c787dceb4b06c696c3fe4007bd87ba8f3303e25ca"),
  @("stable_r17/final_r17_RX34.dxbc","abb86881378803d3389bb42d63c97750865a62e23b1ede205bfb63795855ec30"),
  @("stable_r17/final_r17_RX35.dxbc","023ad52a81399ff81066e55ff223ac9e4a90be7a64e21eb760ac5ba68f38c23b"),
  @("lerp_r19/final_r19_RX33.dxbc","1d9fed8a55689e09bacf55a66a88f2c3474f7c3f9a5604fe5151b0506b06caab"),
  @("lerp_r19/final_r19_RX34.dxbc","8f86b1aec73b3b9ec77c10ce6de2308d0b0d4533a1430373ea92b297ee387f97"),
  @("lerp_r19/final_r19_RX35.dxbc","b5f94ccec4e01e7d98c4329c2b28aecfd65e103729be969a53f486b182c55b70")
 )
 foreach($row in $pins) {
  $file=Join-Path $staging $row[0]
  if(!(Test-Path $file) -or (Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $row[1]){
   throw "V300 full final SHA mismatch: $file"
  }
 }
 $v286=Join-Path $staging "v286"
 Checked "python" @(
  "$repo/renderer-core/tools/v288/atomic_owner_v286_export.py",
  "$bin/v300-v286-lerp.exe",
  "$stable/final_r17_RX33.dxbc","$stable/final_r17_RX34.dxbc","$stable/final_r17_RX35.dxbc",
  "$lerp/final_r19_RX33.dxbc","$lerp/final_r19_RX34.dxbc","$lerp/final_r19_RX35.dxbc",
  $v286)
 $v297=Join-Path $staging "v297_postls"
 Checked "python" @("$repo/renderer-core/tools/v297/patch_lerp_owner_post_ls_domain.py",
   "$v286/V288_OWNER_FINAL_MANIFEST.json",$v286,$v297)
 @("33","34","35") | ForEach-Object {
   $rx=$_
   foreach($candidate in @("$stable/final_r17_RX$rx.dxbc","$lerp/final_r19_RX$rx.dxbc",
         "$v286/v286_R5_R11_R7_LerpRX$rx.dxbc","$v297/v286_R5_R11_R7_LerpRX$rx.dxbc")){
     $stock=if($candidate.StartsWith($stable,[StringComparison]::OrdinalIgnoreCase)){"$original/stable$rx.dxbc"}else{"$original/lerp$rx.dxbc"}
     Checked "$bin/v300-final-r17-exporter.exe" @("--driver-compare",$stock,$candidate)
   }
 }
 $receipt=@{
   schema="V300_OWNER_LOCAL_SHADER_TRIPLET_VERIFIED"
   ready_for_runtime_approval=$false
   pixel_equivalent=$false
   fog="stock_DSR"
   latest_hook_source="PROTECTED"
   files=@(Get-ChildItem $staging -Recurse -Filter "*.dxbc" | ForEach-Object {
     @{path=$_.FullName.Substring($staging.Length+1);size=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
   })
 }
 $receipt | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 "$staging/V300_OWNER_LOCAL_RECEIPT.json"
 Move-Item -LiteralPath $staging -Destination $target
 Write-Host "V300_ALL_OR_NOTHING_OWNER_FINAL_CONSTRUCTION_PASS $target"
}catch{
 Remove-Item $staging -Recurse -Force -ErrorAction SilentlyContinue
 throw
}
