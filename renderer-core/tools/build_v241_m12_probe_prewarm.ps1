# V241 exact m12 PTDE cube 130/40 cold-resource prewarm A/B.
# The V237 synced render operators, no-QPC flags and receiver remain exact.
# Temporary native overlay only, source restored after build.
$ErrorActionPreference = 'Stop'
$path = 'renderer-core/src/runtime/envspec_resource_runtime.cpp'
python renderer-core/tools/test_v241_m12_probe_prewarm.py
if ($LASTEXITCODE -ne 0) { throw 'V241 source tests failed' }
./renderer-core/tools/build_v237_v236_no_profiler.ps1
if ($LASTEXITCODE -ne 0) { throw 'V241 V237 baseline failed' }
$saved = [IO.File]::ReadAllText((Join-Path (Get-Location) $path))
try {
  python renderer-core/tools/apply_v241_m12_probe_prewarm.py --apply
  if ($LASTEXITCODE -ne 0) { throw 'V241 patch failed' }
  python renderer-core/tools/apply_v241_m12_probe_prewarm.py
  if ($LASTEXITCODE -ne 0) { throw 'V241 patched source guard failed' }
  cmake --build build --config Release --target dsrrl_renderer_core_integrated
  if ($LASTEXITCODE -ne 0) { throw 'V241 native link failed' }
  $fresh = 'build/Release/DSRRL_Restored_Lighting_v2_0_2.addon64'
  if (!(Test-Path $fresh)) { throw 'V241 missing freshly linked addon' }
  $binary = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($fresh))
  if (!$binary.Contains('[DSRRL V241 PROBE PREWARM AB]')) { throw 'V241 code not present in linked addon' }
  if ($binary.Contains('[DSRRL HITCH FRAME]') -or $binary.Contains('[DSRRL STUTTER R43]')) {
    throw 'V241 QPC unexpectedly enabled'
  }
  foreach ($flag in @('DSRRL_STUTTER_PROFILE','DSRRL_STUTTER_HITCH_TRACE')) {
    if (!(Select-String -Path 'build/CMakeCache.txt' -SimpleMatch -Pattern ($flag + ':BOOL=OFF') -Quiet)) {
      throw "V241 wrong CMake instrumentation state: $flag"
    }
  }
  Copy-Item $fresh DSRRL_V241_M12_PROBE_130_40_PREWARM_V237_NOQPC.addon64 -Force
  Write-Host 'V241_SINGLE_ADDON_NOQPC_EXACT_PAIR_PREWARM_BUILD_PASS'
} finally {
  [IO.File]::WriteAllText((Join-Path (Get-Location) $path),$saved)
  if ([IO.File]::ReadAllText((Join-Path (Get-Location) $path)) -cne $saved) {
    throw 'V241 source restore failed'
  }
}
