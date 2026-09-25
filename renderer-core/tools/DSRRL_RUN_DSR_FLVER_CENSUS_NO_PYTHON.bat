@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title DSRRL - DSR FLVER Census

set "SELF=%~f0"
set "PS1=%TEMP%\dsrrl_dsr_flver_census_%RANDOM%_%RANDOM%.ps1"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$s=Get-Content -LiteralPath $env:SELF; $m='#==DSRRL_POWERSHELL=='; $i=[Array]::IndexOf([string[]]$s,$m); if($i -lt 0){exit 9}; $s[($i+1)..($s.Length-1)] | Set-Content -LiteralPath $env:PS1 -Encoding UTF8"
if errorlevel 1 (
  echo [ERROR] Could not extract embedded PowerShell.
  pause
  exit /b 9
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%PS1%" -BaseDir "%~dp0" -ExplicitZip "%~1"
set "RC=%ERRORLEVEL%"
del /q "%PS1%" >nul 2>&1

echo.
if "%RC%"=="0" (
  echo [OK] Census complete.
) else (
  echo [ERROR] Census failed with exit code %RC%.
)
echo.
pause
exit /b %RC%

#==DSRRL_POWERSHELL==
param([string]$BaseDir,[string]$ExplicitZip)
Set-StrictMode -Version 2
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Fail([string]$Message,[int]$Code=1){ Write-Host ''; Write-Host ('[ERROR] '+$Message) -ForegroundColor Red; exit $Code }
function Add-Count([hashtable]$S,[string]$K,[long]$A=1){ if(-not $S.ContainsKey($K)){$S[$K]=[long]0}; $S[$K]=[long]$S[$K]+$A }
function Get-BytesSha256([byte[]]$B){$h=[Security.Cryptography.SHA256]::Create();try{(($h.ComputeHash($B)|%{$_.ToString('x2')})-join '')}finally{$h.Dispose()}}
function Get-FileSha256([string]$P){(Get-FileHash -LiteralPath $P -Algorithm SHA256).Hash.ToLowerInvariant()}
function Test-Flver([byte[]]$D){$D.Length-ge 6 -and $D[0]-eq 0x46 -and $D[1]-eq 0x4C -and $D[2]-eq 0x56 -and $D[3]-eq 0x45 -and $D[4]-eq 0x52 -and $D[5]-eq 0}
function Test-Dcx([byte[]]$D){$D.Length-ge 4 -and $D[0]-eq 0x44 -and $D[1]-eq 0x43 -and $D[2]-eq 0x58 -and $D[3]-eq 0}
function Test-AsciiAt([byte[]]$D,[int]$O,[string]$T){if($O-lt 0 -or $O+$T.Length-gt $D.Length){return $false};for($i=0;$i-lt $T.Length;$i++){if($D[$O+$i]-ne [byte][char]$T[$i]){return $false}};return $true}
function Get-I32([byte[]]$D,[int]$O,[bool]$BE){
 if($O-lt 0 -or $O+4-gt $D.Length){throw 'i32 out of range'}
 if(-not $BE){return [BitConverter]::ToInt32($D,$O)}
 [uint32]$u=([uint32]$D[$O]-shl 24)-bor([uint32]$D[$O+1]-shl 16)-bor([uint32]$D[$O+2]-shl 8)-bor([uint32]$D[$O+3])
 if($u-ge 0x80000000){return [int64]$u-0x100000000L};return [int64]$u
}
function Read-Str([byte[]]$D,[int]$O,[bool]$U,[bool]$BE){
 if($O-le 0 -or $O-ge $D.Length){return ''}
 if($U){$e=$O;$c=[Math]::Min($D.Length,$O+8192);while($e+1-lt $c){if($D[$e]-eq 0 -and $D[$e+1]-eq 0){break};$e+=2};$n=$e-$O;if($n-le 0){return ''};if($BE){$enc=[Text.Encoding]::BigEndianUnicode}else{$enc=[Text.Encoding]::Unicode};return $enc.GetString($D,$O,$n)}
 $e=$O;$c=[Math]::Min($D.Length,$O+8192);while($e-lt $c -and $D[$e]-ne 0){$e++};$n=$e-$O;if($n-le 0){return ''};try{$enc=[Text.Encoding]::GetEncoding(932)}catch{$enc=[Text.Encoding]::Default};return $enc.GetString($D,$O,$n)
}
function Expand-Dcx([byte[]]$D){
 if($D.Length-lt 0x50 -or -not(Test-Dcx $D)){throw 'not DCX'}
 if(-not(Test-AsciiAt $D 0x18 "DCS`0") -or -not(Test-AsciiAt $D 0x24 "DCP`0") -or -not(Test-AsciiAt $D 0x28 'DFLT') -or -not(Test-AsciiAt $D 0x44 "DCA`0")){throw 'unsupported DCX'}
 [int]$u=([int]$D[0x1C]-shl 24)-bor([int]$D[0x1D]-shl 16)-bor([int]$D[0x1E]-shl 8)-bor[int]$D[0x1F]
 [int]$c=([int]$D[0x20]-shl 24)-bor([int]$D[0x21]-shl 16)-bor([int]$D[0x22]-shl 8)-bor[int]$D[0x23]
 $o=0x4C;if($c-le 6 -or $o+$c-gt $D.Length){throw 'bad DCX range'}
 $n=$c-6;[byte[]]$raw=New-Object byte[] $n;[Array]::Copy($D,$o+2,$raw,0,$n)
 $src=New-Object IO.MemoryStream(,$raw);$dst=New-Object IO.MemoryStream;$def=New-Object IO.Compression.DeflateStream($src,[IO.Compression.CompressionMode]::Decompress)
 try{$def.CopyTo($dst);[byte[]]$out=$dst.ToArray()}finally{$def.Dispose();$src.Dispose();$dst.Dispose()}
 if($u-gt 0 -and $out.Length-ne $u){throw 'DCX size mismatch'};return ,$out
}
function Unwrap-Dcx([byte[]]$D){$l=0;while(Test-Dcx $D){if($l-ge 4){throw 'too many DCX layers'};$D=Expand-Dcx $D;$l++};[pscustomobject]@{Data=$D;Layers=$l}}
function Parse-Flver([byte[]]$D){
 if($D.Length-lt 0x80 -or -not(Test-Flver $D)){throw 'not FLVER2'}
 if($D[6]-eq 0x4C -and $D[7]-eq 0){$be=$false}elseif($D[6]-eq 0x42 -and $D[7]-eq 0){$be=$true}else{throw 'bad endian'}
 $v=[int](Get-I32 $D 8 $be);if($v-lt 0x20000){throw 'FLVER0 unsupported'}
 $dc=[int](Get-I32 $D 0x14 $be);$mc=[int](Get-I32 $D 0x18 $be);$bc=[int](Get-I32 $D 0x1C $be);$mesh=[int](Get-I32 $D 0x20 $be);$vb=[int](Get-I32 $D 0x24 $be);$fs=[int](Get-I32 $D 0x50 $be);$lo=[int](Get-I32 $D 0x54 $be);$tc=[int](Get-I32 $D 0x58 $be)
 foreach($x in @($dc,$mc,$bc,$mesh,$vb,$fs,$lo,$tc)){if($x-lt 0 -or $x-gt 1000000){throw 'implausible count'}}
 $uni=$D[0x49]-ne 0;[long]$mo=0x80+[long]$dc*0x40;[long]$bo=$mo+[long]$mc*0x20;[long]$meo=$bo+[long]$bc*0x80;[long]$fso=$meo+[long]$mesh*0x30;if($v-gt 0x20005){$fss=0x20}else{$fss=0x1C};[long]$vbo=$fso+[long]$fs*$fss;[long]$loo=$vbo+[long]$vb*0x20;[long]$to=$loo+[long]$lo*0x10
 if($to-lt 0x80 -or $to+[long]$tc*0x20-gt $D.Length){throw 'tables out of range'}
 $hdr=New-Object 'System.Collections.Generic.List[object]'
 for($s=0;$s-lt $mc;$s++){$o=[int]($mo+[long]$s*0x20);$no=[int](Get-I32 $D $o $be);$mt=[int](Get-I32 $D ($o+4) $be);$cnt=[int](Get-I32 $D ($o+8) $be);$ix=[int](Get-I32 $D ($o+12) $be);if($cnt-lt 0 -or $ix-lt 0 -or $ix+$cnt-gt $tc){throw 'bad texture range'};$hdr.Add([pscustomobject]@{slot=$s;name=(Read-Str $D $no $uni $be);mtd=(Read-Str $D $mt $uni $be);count=$cnt;index=$ix})}
 $tex=New-Object 'System.Collections.Generic.List[object]'
 for($i=0;$i-lt $tc;$i++){$o=[int]($to+[long]$i*0x20);$po=[int](Get-I32 $D $o $be);$ty=[int](Get-I32 $D ($o+4) $be);$tex.Add([pscustomobject]@{semantic=(Read-Str $D $ty $uni $be);path=(Read-Str $D $po $uni $be)})}
 $mats=New-Object 'System.Collections.Generic.List[object]'
 foreach($h in $hdr){$b=New-Object 'System.Collections.Generic.List[object]';for($j=0;$j-lt $h.count;$j++){$b.Add($tex[$h.index+$j])};$mats.Add([pscustomobject]@{slot=$h.slot;material_name=$h.name;mtd_path=$h.mtd;textures=$b})}
 [pscustomobject]@{Version=$v;Materials=$mats}
}
function Cell([object]$V){if($null-eq $V){return ''};([string]$V).Replace("`t",' ').Replace("`r",' ').Replace("`n",' ')}

if([string]::IsNullOrWhiteSpace($BaseDir)){$BaseDir=Split-Path -Parent $PSCommandPath}
$BaseDir=[IO.Path]::GetFullPath($BaseDir)
if(-not[string]::IsNullOrWhiteSpace($ExplicitZip)){
 if(-not(Test-Path -LiteralPath $ExplicitZip -PathType Leaf)){Fail ('ZIP not found: '+$ExplicitZip) 2};$zip=(Resolve-Path -LiteralPath $ExplicitZip).Path
}else{
 $p=Join-Path $BaseDir 'flvr dsr.zip';if(Test-Path -LiteralPath $p -PathType Leaf){$zip=$p}else{$zs=@(Get-ChildItem -LiteralPath $BaseDir -Filter *.zip -File|Sort-Object Length -Descending);if($zs.Count-eq 0){Fail 'No ZIP next to BAT' 2};$zip=$zs[0].FullName}
}
$out=Join-Path $BaseDir 'DSRRL_DSR_FLVER_CENSUS';New-Item -ItemType Directory -Force -Path $out|Out-Null
$raw=Join-Path $out 'dsr_flver_ownership_v1_raw.jsonl';$tsv=Join-Path $out 'dsr_flver_ownership_v1_materials.tsv';$proj=Join-Path $out 'dsr_flver_ownership_v1_mtd_projection.tsv';$err=Join-Path $out 'dsr_flver_ownership_v1_errors.jsonl';$sum=Join-Path $out 'dsr_flver_ownership_v1_summary.json'
Write-Host '';Write-Host 'DSRRL DSR FLVER MATERIAL-SLOT CENSUS' -ForegroundColor Cyan;Write-Host ('ZIP: '+$zip);Write-Host ('OUT: '+$out);Write-Host ''
$st=@{};$ver=@{};$errors=New-Object 'System.Collections.Generic.List[object]';$rows=New-Object 'System.Collections.Generic.List[object]';$seen=New-Object 'System.Collections.Generic.HashSet[string]';$pr=@{}
$za=[IO.Compression.ZipFile]::OpenRead($zip)
try{
 $es=@($za.Entries|?{-not[string]::IsNullOrEmpty($_.Name)});Add-Count $st 'zip_members' $es.Count;$ix=0
 foreach($e in $es){$ix++;if(($ix%25)-eq 0 -or $ix-eq 1 -or $ix-eq $es.Count){Write-Progress -Activity 'Scanning FLVER' -Status ("$ix/$($es.Count): $($e.FullName)") -PercentComplete ([int](100*$ix/[Math]::Max(1,$es.Count)))};Add-Count $st 'bytes_uncompressed' $e.Length
  try{$s=$e.Open();$ms=New-Object IO.MemoryStream;try{$s.CopyTo($ms);[byte[]]$d=$ms.ToArray()}finally{$s.Dispose();$ms.Dispose()}}catch{$errors.Add([pscustomobject]@{member=$e.FullName;stage='ZIP_READ';error=$_.Exception.Message});continue}
  try{$uw=Unwrap-Dcx $d;[byte[]]$d=$uw.Data;if($uw.Layers-gt 0){Add-Count $st 'dcx_layers' $uw.Layers}}catch{$errors.Add([pscustomobject]@{member=$e.FullName;stage='DCX';error=$_.Exception.Message});continue}
  if(-not(Test-Flver $d)){Add-Count $st 'non_flver_members';continue};Add-Count $st 'flver_instances';$sha=Get-BytesSha256 $d;if($seen.Add($sha)){Add-Count $st 'unique_flver_payloads'}else{Add-Count $st 'duplicate_flver_payloads'}
  try{$pa=Parse-Flver $d}catch{$errors.Add([pscustomobject]@{member=$e.FullName;stage='FLVER_PARSE';sha256=$sha;error=$_.Exception.Message});continue};Add-Count $st 'flver_parse_ok';$vk=('0x{0:X}' -f $pa.Version);if(-not$ver.ContainsKey($vk)){$ver[$vk]=0};$ver[$vk]=[int]$ver[$vk]+1;$mem=$e.FullName.Replace('\','/');$fid='DSR|'+$mem+'#'+$sha
  foreach($m in $pa.Materials){Add-Count $st 'material_instances';$mp=([string]$m.mtd_path).Replace('\','/');$mn=[IO.Path]::GetFileName($mp);$pk=$mn.ToLowerInvariant();if(-not$pr.ContainsKey($pk)){$pr[$pk]=[pscustomobject]@{Display=$mn;Flvers=(New-Object 'System.Collections.Generic.HashSet[string]');Materials=0;Paths=(New-Object 'System.Collections.Generic.HashSet[string]');Semantics=@{}}};$q=$pr[$pk];[void]$q.Flvers.Add($fid);[void]$q.Paths.Add($mp);$q.Materials=[int]$q.Materials+1
   if($m.textures.Count-eq 0){Add-Count $st 'materials_without_textures';$rows.Add([pscustomobject]@{sort=$fid+'|'+('{0:D8}'-f $m.slot)+'|'+$mn.ToLowerInvariant()+'||';game='DSR';flver_identity=$fid;flver_member=$mem;flver_sha256=$sha;flver_version=$vk;material_slot=$m.slot;material_name=$m.material_name;mtd_path=$mp;mtd_name=$mn;texture_semantic='';texture_path=''})}
   else{Add-Count $st 'texture_bindings' $m.textures.Count;foreach($t in $m.textures){$se=[string]$t.semantic;$tp=([string]$t.path).Replace('\','/');if($se){if(-not$q.Semantics.ContainsKey($se)){$q.Semantics[$se]=0};$q.Semantics[$se]=[int]$q.Semantics[$se]+1};$rows.Add([pscustomobject]@{sort=$fid+'|'+('{0:D8}'-f $m.slot)+'|'+$mn.ToLowerInvariant()+'|'+$se+'|'+$tp.ToLowerInvariant();game='DSR';flver_identity=$fid;flver_member=$mem;flver_sha256=$sha;flver_version=$vk;material_slot=$m.slot;material_name=$m.material_name;mtd_path=$mp;mtd_name=$mn;texture_semantic=$se;texture_path=$tp})}}
  }
 }
 Write-Progress -Activity 'Scanning FLVER' -Completed
}finally{$za.Dispose()}
$rws=@($rows|Sort-Object sort);$utf=New-Object Text.UTF8Encoding($false)
$w=New-Object IO.StreamWriter($raw,$false,$utf);try{foreach($r in $rws){$o=[ordered]@{game=$r.game;flver_identity=$r.flver_identity;flver_member=$r.flver_member;flver_sha256=$r.flver_sha256;flver_version=$r.flver_version;material_slot=$r.material_slot;material_name=$r.material_name;mtd_path=$r.mtd_path;mtd_name=$r.mtd_name;texture_semantic=$r.texture_semantic;texture_path=$r.texture_path};$w.WriteLine(($o|ConvertTo-Json -Compress))}}finally{$w.Dispose()}
$w=New-Object IO.StreamWriter($tsv,$false,$utf);try{$w.WriteLine("game`tflver_identity`tflver_member`tflver_sha256`tflver_version`tmaterial_slot`tmaterial_name`tmtd_path`tmtd_name`ttexture_semantic`ttexture_path");foreach($r in $rws){$v=@($r.game,$r.flver_identity,$r.flver_member,$r.flver_sha256,$r.flver_version,$r.material_slot,$r.material_name,$r.mtd_path,$r.mtd_name,$r.texture_semantic,$r.texture_path)|%{Cell $_};$w.WriteLine(($v-join "`t"))}}finally{$w.Dispose()}
$w=New-Object IO.StreamWriter($proj,$false,$utf);try{$w.WriteLine("mtd_name`tflver_identity_count`tmaterial_instance_count`tmtd_path_count`ttexture_semantics_json");foreach($k in @($pr.Keys|Sort-Object)){$q=$pr[$k];$so=[ordered]@{};foreach($sk in @($q.Semantics.Keys|Sort-Object)){$so[$sk]=$q.Semantics[$sk]};$v=@($q.Display,$q.Flvers.Count,$q.Materials,$q.Paths.Count,($so|ConvertTo-Json -Compress))|%{Cell $_};$w.WriteLine(($v-join "`t"))}}finally{$w.Dispose()}
$w=New-Object IO.StreamWriter($err,$false,$utf);try{foreach($x in $errors){$w.WriteLine(($x|ConvertTo-Json -Compress))}}finally{$w.Dispose()}
$c=[ordered]@{};foreach($k in @($st.Keys|Sort-Object)){$c[$k]=[long]$st[$k]};$c['canonical_rows']=[long]$rws.Count;$c['parse_errors']=[long]$errors.Count;$c['unique_mtd_names']=[long]$pr.Count;$vo=[ordered]@{};foreach($k in @($ver.Keys|Sort-Object)){$vo[$k]=$ver[$k]};$outs=[ordered]@{};foreach($p in @($raw,$tsv,$proj,$err)){$outs[[IO.Path]::GetFileName($p)]=Get-FileSha256 $p}
$summary=[ordered]@{schema=1;game='DSR';claim_scope='FLVER_MATERIAL_SLOT_TEXTURE_BINDING_CONSTRUCTION_EVIDENCE';input=[ordered]@{name=[IO.Path]::GetFileName($zip);size_bytes=(Get-Item -LiteralPath $zip).Length;sha256=(Get-FileSha256 $zip)};counts=$c;flver_versions=$vo;outputs=$outs;policy=[ordered]@{missing_texture='UNKNOWN_NOT_NO_USE';mtd_sha256='UNRESOLVED_UNTIL_EXACT_DSR_MTD_JOIN';cross_version_homology='OPEN';runtime_activation='OPEN';pixel_equivalence='OPEN'}}
[IO.File]::WriteAllText($sum,(($summary|ConvertTo-Json -Depth 8)+"`r`n"),$utf)
Write-Host '';Write-Host 'CENSUS COMPLETE' -ForegroundColor Green;Write-Host ('FLVER instances: '+$c['flver_instances']);Write-Host ('Unique FLVER payloads: '+$c['unique_flver_payloads']);Write-Host ('Material instances: '+$c['material_instances']);Write-Host ('Texture bindings: '+$c['texture_bindings']);Write-Host ('Canonical rows: '+$c['canonical_rows']);Write-Host ('Unique MTD names: '+$c['unique_mtd_names']);Write-Host ('Parser errors: '+$c['parse_errors']);Write-Host ('Results: '+$out)
if(-not$st.ContainsKey('flver_instances') -or [long]$st['flver_instances']-eq 0){exit 2};exit 0
