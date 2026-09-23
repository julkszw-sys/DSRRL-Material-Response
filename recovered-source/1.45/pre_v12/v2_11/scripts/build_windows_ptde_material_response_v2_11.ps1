param([string]$ReShadeSource = "")
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$Pin='aae2b7ecc18096ccddca2c073b50727541220292'; $Api=20
$Root=Split-Path -Parent $PSScriptRoot
$Log=Join-Path $Root 'BUILD_PTDE_MATERIAL_RESPONSE_V2_11.log'
try { Start-Transcript -Path $Log -Force | Out-Null } catch {}
$Build=Join-Path $Root 'build-msvc'; $Bin=Join-Path $Build 'bin'
$NativeBase = if ($env:TEMP) { $env:TEMP } elseif ($env:LOCALAPPDATA) { $env:LOCALAPPDATA } else { $Root }
$NativeBuild=Join-Path $NativeBase 'DSRRL\PTDE_MATERIAL_RESPONSE_V2_11'
$Obj=Join-Path $NativeBuild 'obj'; $NativeBin=Join-Path $NativeBuild 'bin'
$DepsBase = if ($env:LOCALAPPDATA) { $env:LOCALAPPDATA } elseif ($env:TEMP) { $env:TEMP } else { $Root }
$Deps=Join-Path $DepsBase 'DSRRL\PTDE_MATERIAL_RESPONSE_V2_11_deps'
function Invoke-Native { param([string]$Exe,[string[]]$ArgumentList); & $Exe @ArgumentList; if($LASTEXITCODE-ne0){throw "Native command failed ($LASTEXITCODE): $Exe"} }
function Vcvars(){ $v="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"; if(Test-Path $v){$i=&$v -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath|Select-Object -First 1;if($i){return Join-Path $i 'VC\Auxiliary\Build\vcvars64.bat'}};return $null }
if(-not(Get-Command cl.exe -ErrorAction SilentlyContinue) -or -not(Get-Command ml64.exe -ErrorAction SilentlyContinue)){
    $vc=Vcvars; if(-not$vc){throw 'Install VS 2022 Build Tools: MSVC v143 x64/x86 + Windows SDK.'}
    $tmp=Join-Path $env:TEMP ('dsrrl_'+[guid]::NewGuid().ToString('N')+'.cmd'); Set-Content $tmp "@echo off`r`ncall `"$vc`" >nul 2>&1`r`nset" -Encoding ASCII
    $lines=&$env:ComSpec /d /s /c ('"'+$tmp+'"'); Remove-Item $tmp -Force
    foreach($l in $lines){$p=$l.IndexOf('=');if($p-gt0){[Environment]::SetEnvironmentVariable($l.Substring(0,$p),$l.Substring($p+1),'Process')}}
}
$cl=(Get-Command cl.exe).Source; $link=(Get-Command link.exe).Source; $ml64=(Get-Command ml64.exe).Source
function TestHeader([string]$inc){$h=Join-Path $inc 'reshade.hpp';if(-not(Test-Path $h)){return $false};if((Get-Content $h -Raw)-notmatch ('#define\s+RESHADE_API_VERSION\s+'+$Api)){throw "ReShade header is not API $Api"};return $true}
function Stage-ShortInclude([string]$SourceInc){ if(-not(TestHeader $SourceInc)){throw 'Invalid ReShade include'}; $hp=Join-Path $SourceInc 'reshade.hpp'; if($hp.Length-lt220){return $SourceInc}; $short=Join-Path $env:TEMP ('DSRRL_RS20_'+$Pin.Substring(0,10)); $dst=Join-Path $short 'include'; if(Test-Path $short){Remove-Item $short -Recurse -Force}; New-Item -ItemType Directory -Force $dst|Out-Null; Copy-Item (Join-Path $SourceInc '*') $dst -Recurse -Force; if(-not(TestHeader $dst)){throw 'Short include staging failed'}; return $dst }
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$inc=$null
foreach($s in @($ReShadeSource,$env:RESHADE_SOURCE_DIR)){if($s){if(TestHeader (Join-Path $s 'include')){$inc=Join-Path $s 'include';break};if(TestHeader $s){$inc=$s;break}}}
if(-not$inc){New-Item -ItemType Directory -Force $Deps|Out-Null;$cache=Join-Path $Deps ('rs-'+$Pin.Substring(0,10));if(-not(Test-Path $cache)){$z=$cache+'.zip';Invoke-WebRequest -UseBasicParsing -Uri ("https://github.com/crosire/reshade/archive/$Pin.zip") -OutFile $z;New-Item -ItemType Directory -Force $cache|Out-Null;Expand-Archive $z $cache -Force;Remove-Item $z -Force};$h=Get-ChildItem $cache -Recurse -Filter reshade.hpp|Where-Object{$_.Directory.Name-eq'include'}|Select-Object -First 1;if(-not$h){throw 'reshade.hpp not found'};$inc=$h.Directory.FullName}
$inc=Stage-ShortInclude $inc
if(Test-Path $Build){Remove-Item $Build -Recurse -Force}; if(Test-Path $NativeBuild){Remove-Item $NativeBuild -Recurse -Force}
New-Item -ItemType Directory -Force $Bin,$Obj,$NativeBin|Out-Null
$addonSrc=Join-Path $NativeBuild 'addon.cpp'; Copy-Item (Join-Path $Root 'src\addon\addon.cpp') $addonSrc -Force
$asmSrc=Join-Path $NativeBuild 'selector_hook.asm'; Copy-Item (Join-Path $Root 'src\addon\selector_hook.asm') $asmSrc -Force
$shaSrc=Join-Path $NativeBuild 'sha256.cpp'; Copy-Item (Join-Path $Root 'src\core\sha256.cpp') $shaSrc -Force
$addonObj=Join-Path $Obj 'addon.obj'; $asmObj=Join-Path $Obj 'selector_hook.obj'; $shaObj=Join-Path $Obj 'sha256.obj'; $includeRoot=Join-Path $Root 'include'
$common=@('/nologo','/c','/std:c++20','/O2','/EHsc','/W4','/permissive-','/MD','/DNDEBUG','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/GS','/guard:cf','/Zc:__cplusplus','/utf-8',"/I$inc","/I$includeRoot")
Invoke-Native $cl ($common + @($addonSrc,"/Fo$addonObj")); Invoke-Native $cl ($common + @($shaSrc,"/Fo$shaObj")); Invoke-Native $ml64 @('/nologo','/c',"/Fo$asmObj",$asmSrc)
$outNative=Join-Path $NativeBin 'DSRRL_PTDE_MATERIAL_RESPONSE_V2_11.addon64'
Invoke-Native $link @('/nologo','/DLL','/MACHINE:X64',"/OUT:$outNative",'/NOIMPLIB','/NOEXP','/INCREMENTAL:NO',$addonObj,$asmObj,$shaObj,'/DYNAMICBASE','/NXCOMPAT','/GUARD:CF','/OPT:REF','/OPT:ICF')
if(-not(Test-Path $outNative)){throw 'addon64 not created'}
$out=Join-Path $Bin 'DSRRL_PTDE_MATERIAL_RESPONSE_V2_11.addon64'; Copy-Item $outNative $out -Force
function Get-PeInfo([string]$Path){
    $fs=[System.IO.File]::Open($Path,[System.IO.FileMode]::Open,[System.IO.FileAccess]::Read,[System.IO.FileShare]::Read)
    try {
        $br=New-Object System.IO.BinaryReader($fs)
        if($br.ReadUInt16() -ne 0x5A4D){throw 'Output is not an MZ executable'}
        $fs.Position=0x3C; $peOff=$br.ReadInt32()
        if($peOff -lt 0x40 -or $peOff -gt ($fs.Length-24)){throw 'Invalid PE header offset'}
        $fs.Position=$peOff
        if($br.ReadUInt32() -ne 0x00004550){throw 'PE signature missing'}
        $machine=$br.ReadUInt16(); [void]$br.ReadUInt16(); [void]$br.ReadUInt32(); [void]$br.ReadUInt32(); [void]$br.ReadUInt32(); [void]$br.ReadUInt16(); $characteristics=$br.ReadUInt16()
        [pscustomobject]@{ machine=$machine; characteristics=$characteristics; is_x64=($machine -eq 0x8664); is_dll=(($characteristics -band 0x2000) -ne 0) }
    } finally { $fs.Dispose() }
}
$pe=Get-PeInfo $out
if(-not $pe.is_x64){throw ('Output PE machine is not AMD64: 0x{0:X4}' -f $pe.machine)}
if(-not $pe.is_dll){throw ('Output PE is not marked DLL: characteristics=0x{0:X4}' -f $pe.characteristics)}
$hash=(Get-FileHash -Algorithm SHA256 $out).Hash.ToLowerInvariant(); $fi=Get-Item $out
[ordered]@{schema='DSRRL_PTDE_MATERIAL_RESPONSE_V2_11_BUILD_RESULT';status='PASS_NATIVE_COMPILE';build_utc=[DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ');output='build-msvc\bin\DSRRL_PTDE_MATERIAL_RESPONSE_V2_11.addon64';output_size=[Int64]$fi.Length;output_sha256=$hash;reshade_api=$Api;reshade_pin=$Pin;runtime_scope='PTDE_C100_DIFFUSE_LINEAR_PLUS_C101_ROOT_GAIN_UNBOUNDED_F0__STOCK_DSR_SPEC_POW_PBL_ENVSPEC__HEMENVLERP_STOCK';param_changes=$false;ul_changes=$false;probe_substitution=$false;pointlight_receiver_changes=$false;postprocess_changes=$false;pe_machine=('0x{0:X4}' -f $pe.machine);pe_x64=$true;pe_dll=$true;runtime_fidelity='OPEN_UNTIL_GAME_LOAD'} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $Build 'BUILD_RESULT.json') -Encoding UTF8
Write-Host "PASS: $out"; Write-Host "SHA256: $hash"; Write-Host "Build log: $Log"; try { Stop-Transcript | Out-Null } catch {}
