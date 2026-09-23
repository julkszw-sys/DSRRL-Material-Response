param([string]$ReShadeSource = "")
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$Pin='aae2b7ecc18096ccddca2c073b50727541220292'; $Api=20
$Root=Split-Path -Parent $PSScriptRoot
$Build=Join-Path $Root 'build-msvc'; $Bin=Join-Path $Build 'bin'
$NativeBase = if ($env:TEMP) { $env:TEMP } elseif ($env:LOCALAPPDATA) { $env:LOCALAPPDATA } else { $Root }
$NativeBuild=Join-Path $NativeBase 'DSRRL\REAL_PTDE_UL_LIVE'
$Obj=Join-Path $NativeBuild 'obj'; $NativeBin=Join-Path $NativeBuild 'bin'
$DepsBase = if ($env:LOCALAPPDATA) { $env:LOCALAPPDATA } elseif ($env:TEMP) { $env:TEMP } else { $Root }
$Deps=Join-Path $DepsBase 'DSRRL\REAL_PTDE_UL_LIVE_deps'
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
$inc=$null
foreach($s in @($ReShadeSource,$env:RESHADE_SOURCE_DIR)){if($s){if(TestHeader (Join-Path $s 'include')){$inc=Join-Path $s 'include';break};if(TestHeader $s){$inc=$s;break}}}
if(-not$inc){New-Item -ItemType Directory -Force $Deps|Out-Null;$cache=Join-Path $Deps ('rs-'+$Pin.Substring(0,10));if(-not(Test-Path $cache)){$z=$cache+'.zip';Invoke-WebRequest -UseBasicParsing -Uri ("https://github.com/crosire/reshade/archive/$Pin.zip") -OutFile $z;New-Item -ItemType Directory -Force $cache|Out-Null;Expand-Archive $z $cache -Force;Remove-Item $z -Force};$h=Get-ChildItem $cache -Recurse -Filter reshade.hpp|Where-Object{$_.Directory.Name-eq'include'}|Select-Object -First 1;if(-not$h){throw 'reshade.hpp not found'};$inc=$h.Directory.FullName}
$inc=Stage-ShortInclude $inc
Write-Host ("ReShade API20 include: {0} (reshade.hpp path length {1})" -f $inc,(Join-Path $inc 'reshade.hpp').Length)
if(Test-Path $Build){Remove-Item $Build -Recurse -Force}; if(Test-Path $NativeBuild){Remove-Item $NativeBuild -Recurse -Force}
New-Item -ItemType Directory -Force $Bin,$Obj,$NativeBin|Out-Null
$addonSrc=Join-Path $NativeBuild 'addon.cpp'; Copy-Item (Join-Path $Root 'src\addon\addon.cpp') $addonSrc -Force
$asmSrc=Join-Path $NativeBuild 'selector_hook.asm'; Copy-Item (Join-Path $Root 'src\addon\selector_hook.asm') $asmSrc -Force
$shaSrc=Join-Path $NativeBuild 'sha256.cpp'; Copy-Item (Join-Path $Root 'src\core\sha256.cpp') $shaSrc -Force
$dxbcSrc=Join-Path $NativeBuild 'dxbc_checksum.cpp'; Copy-Item (Join-Path $Root 'src\core\dxbc_checksum.cpp') $dxbcSrc -Force
$addonObj=Join-Path $Obj 'addon.obj'; $asmObj=Join-Path $Obj 'selector_hook.obj'; $shaObj=Join-Path $Obj 'sha256.obj'; $dxbcObj=Join-Path $Obj 'dxbc_checksum.obj'; $includeRoot=Join-Path $Root 'include'
Write-Host ("Native staging: {0}" -f $NativeBuild)
$common=@('/nologo','/c','/std:c++20','/O2','/EHsc','/W4','/permissive-','/MD','/DNDEBUG','/GS','/guard:cf','/Zc:__cplusplus','/utf-8',"/I$inc","/I$includeRoot")
Invoke-Native $cl ($common + @($addonSrc,"/Fo$addonObj")); Invoke-Native $cl ($common + @($shaSrc,"/Fo$shaObj")); Invoke-Native $cl ($common + @($dxbcSrc,"/Fo$dxbcObj"))
Invoke-Native $ml64 @('/nologo','/c',"/Fo$asmObj",$asmSrc)
$outNative=Join-Path $NativeBin 'DSRRL_REAL_PTDE_UL_LIVE.addon64'
Invoke-Native $link @('/nologo','/DLL','/MACHINE:X64',"/OUT:$outNative",'/NOIMPLIB','/NOEXP','/INCREMENTAL:NO',$addonObj,$asmObj,$shaObj,$dxbcObj,'/DYNAMICBASE','/NXCOMPAT','/GUARD:CF','/OPT:REF','/OPT:ICF')
if(-not(Test-Path $outNative)){throw 'native staged addon64 not created'}
$out=Join-Path $Bin 'DSRRL_REAL_PTDE_UL_LIVE.addon64'; Copy-Item $outNative $out -Force
if(-not(Test-Path $out)){throw 'addon64 copy-back failed'}
Write-Host "PASS: $out"
