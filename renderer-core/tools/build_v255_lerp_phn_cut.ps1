# Reproduce exact V253->V254 generated source, then add read-only V255 cut.
# One .addon64; no permanent renderer source modifications in git.
$ErrorActionPreference='Stop'
$source='renderer-core/tools/build_v254_lerp_multievent.ps1'
$tmp='renderer-core/tools/_v255_generated_native_build.ps1'
if(!(Test-Path $source)){throw 'Missing V254 donor builder'}
if(Test-Path $tmp){throw 'V255 temporary builder exists; abort'}
$s=[IO.File]::ReadAllText($source).Replace("`r`n","`n")
$needle='if($original.IndexOf($anchor) -lt 0 -or'
if($s.Split([string[]]@($needle),[StringSplitOptions]::None).Count -ne 2) {
  throw 'V254 builder insertion anchor ambiguous'
}
$insertion=@'
$replacement += "`n python renderer-core/tools/apply_v255_lerp_phn_cut_diag.py --apply`n if($LASTEXITCODE -ne 0){throw 'V255 PHN cut overlay failed'}`n python renderer-core/tools/apply_v255_lerp_phn_cut_diag.py`n if($LASTEXITCODE -ne 0){throw 'V255 PHN source oracle failed'}"
'@
$s=$s.Replace($needle,$insertion+"`n"+$needle)
if(!$s.Contains('DSRRL_V254_LERP_MULTIEVENT_V253_SAFE.addon64')) {throw 'V254 addon output missing'}
$s=$s.Replace('DSRRL_V254_LERP_MULTIEVENT_V253_SAFE.addon64','DSRRL_V255_LERP_PHN_CUT_V254_SAFE.addon64')
$tag="'[DSRRL V254 LERP SUMMARY]',"
if(!$s.Contains($tag)){throw 'V254 marker guard missing'}
$s=$s.Replace($tag,$tag+"`n  '[DSRRL V255 PHN CARRIER CUT]',")
$s=$s.Replace('V254_LERP_MULTIEVENT_CONSTRUCTION_PASS','V255_LERP_PHN_CUT_CONSTRUCTION_PASS')
$s=$s.Replace('V254_RUNTIME_MULTIEVENT_AND_PHN_PIXEL=OPEN','V255_RUNTIME_PHN_CUT_AND_PIXEL=OPEN')
$s=$s.Replace('V254_SINGLE_ADDON_SHA256=','V255_SINGLE_ADDON_SHA256=')
[IO.File]::WriteAllText($tmp,$s,[Text.UTF8Encoding]::new($false))
try {
  & $tmp
  if($LASTEXITCODE -ne 0){throw 'V255 Windows native build failed'}
} finally {
  Remove-Item $tmp -Force -ErrorAction SilentlyContinue
}
