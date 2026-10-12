$ErrorActionPreference='Stop'
$prod='renderer-core/src/runtime/pmetal_producer_state.cpp'
$tests='renderer-core/tests/pmetal_producer_state_tests.cpp'
python renderer-core/tools/test_v242_full_payload_identity.py
if($LASTEXITCODE -ne 0){throw 'V242 source precondition'}
./renderer-core/tools/build_v237_v236_no_profiler.ps1
if($LASTEXITCODE -ne 0){throw 'V242 inherited V237 failed'}
$savedProd=[IO.File]::ReadAllText($prod)
$savedTests=[IO.File]::ReadAllText($tests)
try {
  python renderer-core/tools/apply_v242_full_payload_identity.py --tests --apply
  if($LASTEXITCODE -ne 0){throw 'V242 test injection'}
  # Negative control: unmodified producer must fail the Release-safe
  # changed EnvDiffuse / PHN cross-thread payload assertion.
  cmake --build build/renderer-core-lib --config Release --target dsrrl_pmetal_producer_state_tests
  if($LASTEXITCODE -ne 0){throw 'V242 baseline test build failed'}
  ctest --test-dir build/renderer-core-lib -C Release -R '^dsrrl_pmetal_producer_state_tests$' --output-on-failure
  if($LASTEXITCODE -eq 0){throw 'V242 negative control unexpectedly PASS on original source'}
  Write-Host 'V242_NEGATIVE_CONTROL_SOURCE_BUG_REPRODUCED'

  python renderer-core/tools/apply_v236_slot_sync.py --apply
  if($LASTEXITCODE -ne 0){throw 'V242 V236 synchronization restore failed'}
  python renderer-core/tools/apply_v242_full_payload_identity.py --apply
  if($LASTEXITCODE -ne 0){throw 'V242 identity patch failed'}
  cmake --build build --config Release --target dsrrl_renderer_core_integrated
  if($LASTEXITCODE -ne 0){throw 'V242 native integrated link failed'}
  cmake --build build/renderer-core-lib --config Release --target dsrrl_pmetal_producer_state_tests
  if($LASTEXITCODE -ne 0){throw 'V242 fixed regression build failed'}
  ctest --test-dir build/renderer-core-lib -C Release -R '^dsrrl_pmetal_producer_state_tests$' --output-on-failure
  if($LASTEXITCODE -ne 0){throw 'V242 complete EnvDiffuse/PHN payload regression failed'}

  $fresh='build/Release/DSRRL_Restored_Lighting_v2_0_2.addon64'
  if(!(Test-Path $fresh)){throw 'V242 no integrated addon'}
  $out='DSRRL_V242_PMETAL_FULL_PAYLOAD_IDENTITY_V237_NOQPC.addon64'
  Copy-Item $fresh $out -Force
  Write-Host 'V242_NEGATIVE_FAIL_POSITIVE_PASS_SINGLE_ADDON'
} finally {
  [IO.File]::WriteAllText($prod,$savedProd)
  [IO.File]::WriteAllText($tests,$savedTests)
}
