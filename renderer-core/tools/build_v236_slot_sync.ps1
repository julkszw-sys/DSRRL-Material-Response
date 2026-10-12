$ErrorActionPreference = 'Stop'
$source = 'renderer-core/src/runtime/pmetal_producer_state.cpp'
./renderer-core/tools/build_v233_transition_stutter_diagnostic.ps1
if ($LASTEXITCODE -ne 0) { throw 'V236 baseline failed' }
$before = [IO.File]::ReadAllText($source)
try {
 python renderer-core/tools/apply_v236_slot_sync.py --apply
 if ($LASTEXITCODE -ne 0) { throw 'V236 patch failed' }
 cmake --build build --config Release --target dsrrl_renderer_core_integrated
 if ($LASTEXITCODE -ne 0) { throw 'V236 native failed' }
 ./renderer-core/tools/verify_v203_clean_equipment.ps1
 if ($LASTEXITCODE -ne 0) { throw 'V236 verify failed' }
 cmake --build build/renderer-core-lib --config Release --target dsrrl_pmetal_producer_state_tests
 if ($LASTEXITCODE -ne 0) { throw 'V236 synchronized producer test build failed' }
 ctest --test-dir build/renderer-core-lib -C Release -R dsrrl_pmetal_producer_state_tests --output-on-failure
 if ($LASTEXITCODE -ne 0) { throw 'V236 synchronized producer test failed' }
 Copy-Item DSRRL_v203_CLEAN_EQUIPMENT_SPEC_EXPERIMENT.addon64 DSRRL_V236_SLOT_SYNC_DIAG.addon64 -Force
} finally {
 [IO.File]::WriteAllText($source,$before)
}
