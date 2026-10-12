# V250 exact P_Metal LERP draw-consumer frontier with complete V249 safety.
# No source patch to the owner-proven hook or PTDE shader equation.
$ErrorActionPreference='Stop'
$donor='044255aa91d53a3270633c25f49d97a8cb2db68d'
git diff --quiet $donor HEAD -- renderer-core/src renderer-core/include renderer-core/integrated renderer-core/shaders
if ($LASTEXITCODE -ne 0) { throw 'V250 protected V249 source changed' }
./renderer-core/tools/build_v249_primary_hook_parity.ps1
if ($LASTEXITCODE -ne 0) { throw 'V250 full V249 baseline failed' }

$paths=@(
 'renderer-core/src/runtime/flver_engine_hooks.cpp',
 'renderer-core/src/runtime/pmetal_env_source_runtime.cpp',
 'renderer-core/src/runtime/pmetal_producer_state.cpp',
 'renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp',
 'renderer-core/integrated/integrated_addon.cpp'
)
$saved=@{}
foreach($path in $paths){ $saved[$path]=[IO.File]::ReadAllText($path) }
try {
  # V249 builder restored its temporary tracked overlays. Re-apply EXACT
  # V236+V245 producer safety as well as V249 identity before final compile.
  python renderer-core/tools/apply_v236_slot_sync.py --apply
  if($LASTEXITCODE -ne 0){throw 'V250 V236 mutex missing'}
  python renderer-core/tools/apply_v245_exact_source_record_rearm.py --apply
  if($LASTEXITCODE -ne 0){throw 'V250 V245 payload missing'}
  python renderer-core/tools/apply_v249_primary_source_join.py --apply
  if($LASTEXITCODE -ne 0){throw 'V250 V249 source join missing'}
  python renderer-core/tools/test_v249_primary_source_join.py
  if($LASTEXITCODE -ne 0){throw 'V250 V249 exact source regression'}

  # An exact PMetal LERP-only bounded reason inside prepare (release-clean
  # mode still ON), plus proof from real on_draw and on_draw_indexed paths.
  python renderer-core/tools/apply_v250_pmetal_lerp_prepare_reason.py --apply
  if($LASTEXITCODE -ne 0){throw 'V250 exact reason overlay rejected'}
  python renderer-core/tools/apply_v250_pmetal_lerp_draw_frontier.py --apply
  if($LASTEXITCODE -ne 0){throw 'V250 exact draw frontier rejected'}
  python renderer-core/tools/apply_v250_pmetal_lerp_prepare_reason.py
  if($LASTEXITCODE -ne 0){throw 'V250 exact reason postcondition FAIL'}
  python renderer-core/tools/apply_v250_pmetal_lerp_draw_frontier.py
  if($LASTEXITCODE -ne 0){throw 'V250 exact draw oracle FAIL'}

  # Physical-cut source is GENERATED at configure-time. Re-materialize
  # while the temporary V250 on_draw overlay is present, not afterwards.
  cmake -S renderer-core/integrated -B build -A x64
  if($LASTEXITCODE -ne 0){throw 'V250 physical-cut regeneration FAIL'}
  $generated=@(Get-ChildItem 'build/generated' -Recurse -Filter integrated_addon_physical_cut.cpp)
  if($generated.Count -ne 1){throw "V250 exact one generated cut missing: $($generated.Count)"}
  $physical=[IO.File]::ReadAllText($generated[0].FullName)
  if(!$physical.Contains('[DSRRL V250 PMETAL LERP DRAW]')){
    throw 'V250 DRAW_ISSUED / failopen evidence NOT in compiled physical-cut source'
  }
  if(!$physical.Contains('v250_pmetal_lerp_draw_proof(')){
    throw 'V250 actual callback attachment missing'
  }
  $source=[IO.File]::ReadAllText('renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp')
  if(!$source.Contains('[DSRRL V250 PMETAL LERP PREPARE]')){
    throw 'V250 PMetal LERP prepared-stage evidence missing'
  }
  $producer=[IO.File]::ReadAllText('renderer-core/src/runtime/pmetal_producer_state.cpp')
  if(!$producer.Contains('same_source_payload(slot.record.source, g_record.source)')){
    throw 'V250 full V245 synchronized source record equality missing'
  }
  $sourceLane=[IO.File]::ReadAllText('renderer-core/src/runtime/pmetal_env_source_runtime.cpp')
  if(!$sourceLane.Contains('[DSRRL V249 PMETAL SOURCE JOIN]')){
    throw 'V250 protected V249 primary/hook join missing'
  }
  $resource=[IO.File]::ReadAllText('renderer-core/src/runtime/material_resource_draw_runtime.cpp')
  foreach($token in @('const std::filesystem::path &process_dir() noexcept',
                      'constexpr std::size_t k_release_batch = 64u',
                      'g_quarantined.store(')){
    if(!$resource.Contains($token)){throw "V250 V230/V231 safety absent: $token"}
  }

  cmake --build build --config Release --target dsrrl_renderer_core_integrated
  if($LASTEXITCODE -ne 0){throw 'V250 Win64 native addon compile failed'}
  cmake --build build/renderer-core-lib --config Release --target dsrrl_pmetal_producer_state_tests
  if($LASTEXITCODE -ne 0){throw 'V250 producer tests compile failed'}
  & 'build/renderer-core-lib/Release/dsrrl_pmetal_producer_state_tests.exe'
  if($LASTEXITCODE -ne 0){throw 'V250 producer native unit FAIL'}

  $cache=[IO.File]::ReadAllText('build/CMakeCache.txt')
  foreach($flag in @('DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE:BOOL=ON',
      'DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW:BOOL=ON',
      'DSRRL_RELEASE_CLEANUP:BOOL=ON')){
    if(!$cache.Contains($flag)){throw "V250 core operator option missing $flag"}
  }
  foreach($flag in @('DSRRL_STUTTER_PROFILE:BOOL=ON',
      'DSRRL_STUTTER_HITCH_TRACE:BOOL=ON')){
    if($cache.Contains($flag)){throw "V250 QPC profiler enabled $flag"}
  }
  $base='build/Release/DSRRL_Restored_Lighting_v2_0_2.addon64'
  if(!(Test-Path $base)){throw 'V250 final integrated binary missing'}
  $out='DSRRL_V250_EXACT_PMETAL_LERP_ISSUE_PROOF_V249_SAFE.addon64'
  Copy-Item $base $out -Force
  $binary=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($out))
  foreach($marker in @(
    '[DSRRL V250 PMETAL LERP DRAW]',
    '[DSRRL V250 PMETAL LERP PREPARE]',
    '[DSRRL V249 PMETAL SOURCE JOIN]',
    '[DSRRL PMETAL R19 LERP]')){
      if(!$binary.Contains($marker)){throw "V250 final binary marker missing $marker"}
  }
  $sha=(Get-FileHash $out -Algorithm SHA256).Hash.ToLowerInvariant()
  Write-Host "V250_SINGLE_ADDON_SHA256=$sha"
  Write-Host 'V250_PMETAL_LERP_TRUE_DRAW_FRONTIER_CONSTRUCTION_PASS'
  Write-Host 'V250_RUNTIME_ISSUE_AND_PIXEL=OPEN'
} finally {
  foreach($path in $paths){[IO.File]::WriteAllText($path,$saved[$path])}
  foreach($path in $paths){
    if([IO.File]::ReadAllText($path) -cne $saved[$path]){
       throw "V250 unsafe source restoration failed: $path"}
  }
}
