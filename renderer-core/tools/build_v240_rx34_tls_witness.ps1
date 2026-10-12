# V240 read-only renderer-ready endpoint witness on exact V237 no-QPC.
$ErrorActionPreference = 'Stop'
$src = 'renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp'
$patch = 'renderer-core/tools/apply_v240_rx34_tls_witness.py'
python renderer-core/tools/test_v240_rx34_tls_witness.py
if ($LASTEXITCODE -ne 0) { throw 'V240 negative source oracle failed' }
./renderer-core/tools/build_v237_v236_no_profiler.ps1
if ($LASTEXITCODE -ne 0) { throw 'V240 inherited V237 native sync/no-QPC build failed' }
$saved = [IO.File]::ReadAllText((Join-Path (Get-Location) $src))
try {
  python $patch --apply
  if ($LASTEXITCODE -ne 0) { throw 'V240 exact source overlay failed' }
  python $patch
  if ($LASTEXITCODE -ne 0) { throw 'V240 source postcondition failed' }
  cmake --build build --config Release --target dsrrl_renderer_core_integrated
  if ($LASTEXITCODE -ne 0) { throw 'V240 native addon rebuild failed' }
  # V237 already passed inherited verifier; check final V240 binary below.
  Write-Host 'V240_NATIVE_REBUILD_COMPLETE'
  foreach ($flag in @('DSRRL_STUTTER_PROFILE','DSRRL_STUTTER_HITCH_TRACE')) {
    if (!(Select-String -Path 'build/CMakeCache.txt' -Pattern ($flag + ':BOOL=OFF') -SimpleMatch -Quiet)) {
      throw "V240 R43 QPC unexpectedly enabled: $flag"
    }
  }
  $base = 'build/Release/DSRRL_Restored_Lighting_v2_0_2.addon64'
  if (!(Test-Path $base)) { throw 'V240 single addon missing' }
  $raw = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($base))
  if (!$raw.Contains('[DSRRL V240 ENDPOINT]')) { throw 'V240 endpoint witness missing in binary' }
  if ($raw.Contains('[DSRRL HITCH FRAME]') -or $raw.Contains('[DSRRL STUTTER R43]')) {
    throw 'V240 stutter QPC unexpectedly linked'
  }
  Copy-Item $base 'DSRRL_V240_RX34_LERP_TLS_WITNESS_V237_NOQPC.addon64' -Force
  Write-Host 'V240_SINGLE_ADDON_RX33_ENDPOINT_DIAGNOSTIC_WITH_V237_SYNC_NOQPC_PASS'
} finally {
  [IO.File]::WriteAllText((Join-Path (Get-Location) $src),$saved)
  if ([IO.File]::ReadAllText((Join-Path (Get-Location) $src)) -cne $saved) {
    throw 'V240 source restore failed'
  }
}
