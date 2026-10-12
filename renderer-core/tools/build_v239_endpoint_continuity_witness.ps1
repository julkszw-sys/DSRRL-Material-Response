# V239 read-only renderer-ready endpoint witness on exact V237 no-QPC.
$ErrorActionPreference = 'Stop'
$src = 'renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp'
$patch = 'renderer-core/tools/apply_v239_endpoint_witness.py'
python renderer-core/tools/test_v239_endpoint_witness.py
if ($LASTEXITCODE -ne 0) { throw 'V239 negative source oracle failed' }
./renderer-core/tools/build_v237_v236_no_profiler.ps1
if ($LASTEXITCODE -ne 0) { throw 'V239 inherited V237 native sync/no-QPC build failed' }
$saved = [IO.File]::ReadAllText((Join-Path (Get-Location) $src))
try {
  python $patch --apply
  if ($LASTEXITCODE -ne 0) { throw 'V239 exact source overlay failed' }
  python $patch
  if ($LASTEXITCODE -ne 0) { throw 'V239 source postcondition failed' }
  cmake --build build --config Release --target dsrrl_renderer_core_integrated
  if ($LASTEXITCODE -ne 0) { throw 'V239 native addon rebuild failed' }
  # V237 already passed inherited verifier; check final V239 binary below.
  Write-Host 'V239_NATIVE_REBUILD_COMPLETE'
  foreach ($flag in @('DSRRL_STUTTER_PROFILE','DSRRL_STUTTER_HITCH_TRACE')) {
    if (!(Select-String -Path 'build/CMakeCache.txt' -Pattern ($flag + ':BOOL=OFF') -SimpleMatch -Quiet)) {
      throw "V239 R43 QPC unexpectedly enabled: $flag"
    }
  }
  $base = 'DSRRL_v203_CLEAN_EQUIPMENT_SPEC_EXPERIMENT.addon64'
  if (!(Test-Path $base)) { throw 'V239 single addon missing' }
  $raw = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($base))
  if (!$raw.Contains('[DSRRL V239 ENDPOINT]')) { throw 'V239 endpoint witness missing in binary' }
  if ($raw.Contains('[DSRRL HITCH FRAME]') -or $raw.Contains('[DSRRL STUTTER R43]')) {
    throw 'V239 stutter QPC unexpectedly linked'
  }
  Copy-Item $base 'DSRRL_V239_HEMENV_LERP_ENDPOINT_CONTINUITY_V237_NOQPC.addon64' -Force
  Write-Host 'V239_SINGLE_ADDON_RX33_ENDPOINT_DIAGNOSTIC_WITH_V237_SYNC_NOQPC_PASS'
} finally {
  [IO.File]::WriteAllText((Join-Path (Get-Location) $src),$saved)
  if ([IO.File]::ReadAllText((Join-Path (Get-Location) $src)) -cne $saved) {
    throw 'V239 source restore failed'
  }
}
