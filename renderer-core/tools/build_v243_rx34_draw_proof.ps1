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
  # Physical-cut builds compile build/generated/integrated_addon_physical_cut.cpp,
  # NOT the source-controlled integrated_addon.cpp itself. Reconfigure while
  # the temporary source overlay is installed so the generated C++ gets it.
  cmake -S renderer-core/integrated -B build -A x64
  if ($LASTEXITCODE -ne 0) { throw 'physical-cut regeneration failed' }
  $generated = @(Get-ChildItem 'build/generated' -Recurse -Filter 'integrated_addon_physical_cut.cpp')
  if ($generated.Count -ne 1) { throw 'V243 exact generated cut source not located' }
  if (!([IO.File]::ReadAllText($generated[0].FullName)).Contains('[DSRRL V243 RX DRAW PROOF]')) { throw 'V243 absent in compiled physical-cut source' }
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
