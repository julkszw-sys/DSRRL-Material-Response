# V245 source-correctness candidate, R43/v2.0.3-derived, V237 no-QPC.
# Neither the current R43 release nor the v2.0.3 prerelease is modified.
$ErrorActionPreference = 'Stop'
$prod = 'renderer-core/src/runtime/pmetal_producer_state.cpp'
$tests = 'renderer-core/tests/pmetal_producer_state_tests.cpp'
python renderer-core/tools/test_v245_exact_source_record_rearm.py
if ($LASTEXITCODE -ne 0) { throw 'V245 source oracle failed' }

./renderer-core/tools/build_v237_v236_no_profiler.ps1
if ($LASTEXITCODE -ne 0) { throw 'V245 reproducible V237 baseline failed' }

$savedProd = [IO.File]::ReadAllText($prod)
$savedTests = [IO.File]::ReadAllText($tests)
try {
  python renderer-core/tools/apply_v245_exact_source_record_rearm.py --tests --apply
  if ($LASTEXITCODE -ne 0) { throw 'V245 conditional unit tests failed to materialize' }
  cmake --build build/renderer-core-lib --config Release --target dsrrl_pmetal_producer_state_tests
  if ($LASTEXITCODE -ne 0) { throw 'V245 baseline unit target compile failed' }
  & 'build/renderer-core-lib/Release/dsrrl_pmetal_producer_state_tests.exe'
  $baselineCode = $LASTEXITCODE
  Write-Host "V245_UNPATCHED_NEGATIVE_CONTROL_EXIT=$baselineCode"
  if ($baselineCode -ne 42 -and $baselineCode -ne 43) {
      throw "V245 baseline does not reproduce missing downstream payload: $baselineCode"
  }

  # The V237 builder restores the V236 patch. V245 must run against exactly
  # that synchronized slot mutex rather than a naked record reader.
  python renderer-core/tools/apply_v236_slot_sync.py --apply
  if ($LASTEXITCODE -ne 0) { throw 'V245 V236 synchronized predecessor absent' }
  python renderer-core/tools/apply_v245_exact_source_record_rearm.py --apply
  if ($LASTEXITCODE -ne 0) { throw 'V245 exact payload/rearm repair failed' }
  python renderer-core/tools/apply_v245_exact_source_record_rearm.py
  if ($LASTEXITCODE -ne 0) { throw 'V245 repaired source postcondition failed' }

  cmake --build build/renderer-core-lib --config Release --target dsrrl_pmetal_producer_state_tests
  if ($LASTEXITCODE -ne 0) { throw 'V245 repaired native unit target compile failed' }
  & 'build/renderer-core-lib/Release/dsrrl_pmetal_producer_state_tests.exe'
  $positiveCode = $LASTEXITCODE
  Write-Host "V245_REPAIRED_POSITIVE_CONTROL_EXIT=$positiveCode"
  if ($positiveCode -ne 0) { throw "V245 repaired source invariant FAILED: $positiveCode" }

  cmake --build build --config Release --target dsrrl_renderer_core_integrated
  if ($LASTEXITCODE -ne 0) { throw 'V245 integrated Win64 addon compile failed' }
  & 'build/renderer-core-lib/Release/dsrrl_pmetal_producer_state_tests.exe'
  if ($LASTEXITCODE -ne 0) { throw 'V245 producer state regressed after addon build' }

  $fresh='build/Release/DSRRL_Restored_Lighting_v2_0_2.addon64'
  if (!(Test-Path $fresh)) { throw 'V245 single integrated addon missing' }
  $binary=[IO.File]::ReadAllBytes($fresh)
  if ($binary.Length -lt 500000) { throw 'V245 binary implausibly small' }
  $out='DSRRL_V245_EXACT_LERP_SOURCE_RECORD_REARM_V237_NOQPC.addon64'
  Copy-Item -LiteralPath $fresh -Destination $out -Force
  Write-Host 'V245_EXACT_REARM_NATIVE_NEGATIVE_FAIL_POSITIVE_PASS'
  Write-Host 'V245_PIXEL_BEHAVIOR_OPEN_NO_OWNER_TEST_REQUIRED_FOR_CONSTRUCTION'
} finally {
  [IO.File]::WriteAllText($prod,$savedProd)
  [IO.File]::WriteAllText($tests,$savedTests)
  if ([IO.File]::ReadAllText($prod) -cne $savedProd -or
      [IO.File]::ReadAllText($tests) -cne $savedTests) {
      throw 'V245 failed to restore exact tracked sources'
  }
}
