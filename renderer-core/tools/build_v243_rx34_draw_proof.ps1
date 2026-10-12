$ErrorActionPreference = 'Stop'
$src = 'renderer-core/integrated/integrated_addon.cpp'
python renderer-core/tools/test_v243_rx34_draw_proof.py
if ($LASTEXITCODE -ne 0) { throw 'source test failed' }
./renderer-core/tools/build_v237_v236_no_profiler.ps1
if ($LASTEXITCODE -ne 0) { throw 'base failed' }
$saved = [IO.File]::ReadAllText($src)
try {
  python renderer-core/tools/apply_v243_rx34_draw_proof.py --apply
  if ($LASTEXITCODE -ne 0) { throw 'patch failed' }
  cmake --build build --config Release --target dsrrl_renderer_core_integrated
  if ($LASTEXITCODE -ne 0) { throw 'native failed' }
  $fresh = 'build/Release/DSRRL_Restored_Lighting_v2_0_2.addon64'
  if (!(Test-Path $fresh)) { throw 'addon missing' }
  $text = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($fresh))
  if (!$text.Contains('[DSRRL V243 RX DRAW PROOF]')) { throw 'diagnostic missing' }
  if ($text.Contains('[DSRRL HITCH FRAME]')) { throw 'QPC still linked' }
  Copy-Item $fresh 'DSRRL_V243_RX34_DRAW_ISSUE_PROOF_V237_NOQPC.addon64' -Force
  Write-Host 'V243_NATIVE_SINGLE_ADDON_COMPLETE'
} finally {
  [IO.File]::WriteAllText($src,$saved)
  if ([IO.File]::ReadAllText($src) -cne $saved) { throw 'restore failed' }
}
