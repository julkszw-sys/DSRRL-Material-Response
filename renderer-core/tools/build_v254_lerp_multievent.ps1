# V254 diagnostic release from pristine V253 lineage: single addon, no source drift.
# This script creates a temporary derivative of the audited V253 native builder.
# V253/V252 overlays happen first, V254 probes immediately before CMake.
$ErrorActionPreference='Stop'
$source='renderer-core/tools/build_v253_ptde_envspec_rgbm.ps1'
$temporary='renderer-core/tools/_v254_generated_native_build.ps1'
if(!(Test-Path $source)){throw 'Missing V253 reproducible builder'}
if(Test-Path $temporary){throw 'V254 temporary builder exists; refusing overwrite'}
$original=[IO.File]::ReadAllText($source).Replace("`r`n","`n")
# Match the actual V253 builder exactly rather than quoting PowerShell syntax.
$anchor=" python renderer-core/tools/apply_v253_ptde_envspec_rgbm.py`n if(`$LASTEXITCODE -ne 0){throw 'V253 PTDE EnvSpec exact source oracle fail'}"
$replacement=$anchor+"`n python renderer-core/tools/apply_v254_lerp_multievent_diag.py --apply`n if(`$LASTEXITCODE -ne 0){throw 'V254 read-only LERP observer overlay failed'}`n python renderer-core/tools/apply_v254_lerp_multievent_diag.py`n if(`$LASTEXITCODE -ne 0){throw 'V254 LERP source oracle failed'}"
if($original.IndexOf($anchor) -lt 0 -or
   $original.IndexOf($anchor, $original.IndexOf($anchor) + 1) -ge 0){
  throw 'V253 expected source oracle anchor absent or ambiguous'
}
$next=$original.Replace($anchor,$replacement)
$oldOut="`$out='DSRRL_V253_EXACT_PTDE_ENVSPEC_RGBM_V252_SAFE.addon64'"
$newOut="`$out='DSRRL_V254_LERP_MULTIEVENT_V253_SAFE.addon64'"
if(!$next.Contains($oldOut)){throw 'V253 output filename anchor absent'}
$next=$next.Replace($oldOut,$newOut)
$marker="  '[DSRRL V253 PTDE ENVSPEC RGBM]',"
if(!$next.Contains($marker)){throw 'V253 marker missing'}
$next=$next.Replace($marker,$marker+"`n  '[DSRRL V254 LERP EVENTS]',`n  '[DSRRL V254 LERP SOURCE]',`n  '[DSRRL V254 LERP SUMMARY]',")
$old='V253_EXACT_PTDE_LIGHTBANK_ENVSPEC_RGBM_CONSTRUCTION_PASS'
$next=$next.Replace($old,'V254_LERP_MULTIEVENT_CONSTRUCTION_PASS')
$old='V253_RUNTIME_RGBM_ACTIVATION_AND_PIXEL=OPEN'
$next=$next.Replace($old,'V254_RUNTIME_MULTIEVENT_AND_PHN_PIXEL=OPEN')
$next=$next.Replace('V253_SINGLE_ADDON_SHA256=','V254_SINGLE_ADDON_SHA256=')
[IO.File]::WriteAllText($temporary,$next,[Text.UTF8Encoding]::new($false))
try {
  & $temporary
  if($LASTEXITCODE -ne 0){throw 'V254 native builder failed'}
} finally {
  Remove-Item $temporary -Force -ErrorAction SilentlyContinue
}
