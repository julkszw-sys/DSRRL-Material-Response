$ErrorActionPreference='Stop'
$prod='renderer-core/src/runtime/pmetal_producer_state.cpp'
$tests='renderer-core/tests/pmetal_producer_state_tests.cpp'
python renderer-core/tools/test_v242_full_payload_identity.py
if ($LASTEXITCODE -ne 0) { throw 'V242 source test failed' }
./renderer-core/tools/build_v237_v236_no_profiler.ps1
if ($LASTEXITCODE -ne 0) { throw 'V242 baseline failed' }
$savedProd=[IO.File]::ReadAllText($prod)
$savedTests=[IO.File]::ReadAllText($tests)
try {
  python renderer-core/tools/apply_v242_full_payload_identity.py --tests --apply
  if ($LASTEXITCODE -ne 0) { throw 'V242 test injection failed' }
  cmake --build build/renderer-core-lib --config Release --target dsrrl_pmetal_producer_state_tests
  if ($LASTEXITCODE -ne 0) { throw 'V242 baseline target compile failed' }
  & 'build/renderer-core-lib/Release/dsrrl_pmetal_producer_state_tests.exe'
  Write-Host "V242_BASELINE_NATIVE_EXIT=$LASTEXITCODE"
  if ($LASTEXITCODE -eq 0) { throw 'V242 baseline unexpectedly passes changed downstream payload' }
  python renderer-core/tools/apply_v236_slot_sync.py --apply
  if ($LASTEXITCODE -ne 0) { throw 'V242 V236 mutex patch failed' }
  python renderer-core/tools/apply_v242_full_payload_identity.py --apply
  if ($LASTEXITCODE -ne 0) { throw 'V242 payload patch failed' }
  cmake --build build --config Release --target dsrrl_renderer_core_integrated
  if ($LASTEXITCODE -ne 0) { throw 'V242 integrated build failed' }
  cmake --build build/renderer-core-lib --config Release --target dsrrl_pmetal_producer_state_tests
  if ($LASTEXITCODE -ne 0) { throw 'V242 repaired test build failed' }
  & 'build/renderer-core-lib/Release/dsrrl_pmetal_producer_state_tests.exe'
  Write-Host "V242_FIXED_NATIVE_EXIT=$LASTEXITCODE"
  if ($LASTEXITCODE -ne 0) { throw 'V242 fixed regression failed' }
  $fresh='build/Release/DSRRL_Restored_Lighting_v2_0_2.addon64'
  if (!(Test-Path $fresh)) { throw 'V242 addon missing' }
  Copy-Item $fresh DSRRL_V242_PMETAL_FULL_PAYLOAD_IDENTITY_V237_NOQPC.addon64 -Force
  Write-Host 'V242_BASELINE_FAIL_FIXED_PASS_NATIVE_ONE_ADDON'
} finally {
  [IO.File]::WriteAllText($prod,$savedProd)
  [IO.File]::WriteAllText($tests,$savedTests)
}
