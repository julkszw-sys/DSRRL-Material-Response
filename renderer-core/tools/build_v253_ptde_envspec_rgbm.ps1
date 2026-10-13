# V253: preserve complete V250 producer/operator resource code and add
# one native D3D11 readback before the exact PMetal LERP draw.
$ErrorActionPreference='Stop'
$donor='33a63a34ad586f111db4dc64712fdb9c76250928'
git diff --quiet $donor HEAD -- renderer-core/src renderer-core/include renderer-core/integrated renderer-core/shaders
if($LASTEXITCODE -ne 0){throw 'V253 protected V250 source changed'}
./renderer-core/tools/build_v252_nonmetal_lerp_mtd_stage.ps1
if($LASTEXITCODE -ne 0){throw 'V253 V250 native baseline failed'}
$paths=@(
 'renderer-core/include/dsrrl/runtime/draw_state_transaction.hpp',
 'renderer-core/src/runtime/draw_state_transaction.cpp',
 'renderer-core/src/runtime/flver_engine_hooks.cpp',
 'renderer-core/src/runtime/pmetal_env_source_runtime.cpp',
 'renderer-core/src/runtime/pmetal_producer_state.cpp',
 'renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp',
 'renderer-core/integrated/integrated_addon.cpp'
)
$original=@{}
foreach($path in $paths){$original[$path]=[IO.File]::ReadAllText($path)}
try{
 python renderer-core/tools/apply_v236_slot_sync.py --apply
 if($LASTEXITCODE -ne 0){throw 'V253 required V236 mutex'}
 python renderer-core/tools/apply_v245_exact_source_record_rearm.py --apply
 if($LASTEXITCODE -ne 0){throw 'V253 required V245 payload'}
 python renderer-core/tools/apply_v249_primary_source_join.py --apply
 if($LASTEXITCODE -ne 0){throw 'V253 required V249 parity'}
 python renderer-core/tools/apply_v250_pmetal_lerp_prepare_reason.py --apply
 if($LASTEXITCODE -ne 0){throw 'V253 required V250 gate'}
 python renderer-core/tools/apply_v250_pmetal_lerp_draw_frontier.py --apply
 if($LASTEXITCODE -ne 0){throw 'V253 required V250 draw issue'}
 python renderer-core/tools/apply_v251_native_bound_state.py --apply
 if($LASTEXITCODE -ne 0){throw 'V253 source overlay failed'}
 python renderer-core/tools/apply_v251_native_bound_state.py
 if($LASTEXITCODE -ne 0){throw 'V253 source oracle failed'}
 python renderer-core/tools/apply_v252_nonmetal_lerp_stage.py --apply
 if($LASTEXITCODE -ne 0){throw 'V253 nonmetal stage overlay rejected' }
 python renderer-core/tools/apply_v252_nonmetal_lerp_stage.py
 if($LASTEXITCODE -ne 0){throw 'V253 nonmetal stage postcondition FAIL' }
 python renderer-core/tools/generate_v253_ptde_envspec_rgbm.py
 if($LASTEXITCODE -ne 0){throw 'V253 authenticated PTDE bank/row data generator fail'}
 python renderer-core/tools/apply_v253_ptde_envspec_rgbm.py --apply
 if($LASTEXITCODE -ne 0){throw 'V253 PTDE EnvSpec consumer overlay fail'}
 python renderer-core/tools/apply_v253_ptde_envspec_rgbm.py
 if($LASTEXITCODE -ne 0){throw 'V253 PTDE EnvSpec exact source oracle fail'}
 $h='renderer-core/include/dsrrl/runtime/generated_v253_ptde_envspec_rgbm.hpp'
 if(!(Test-Path $h) -or !([IO.File]::ReadAllText($h)).Contains('std::array<row,1246>')){
    throw 'V253 PTDE exact RGBM donor header invalid'}
 python renderer-core/tools/test_v249_primary_source_join.py
 if($LASTEXITCODE -ne 0){throw 'V253 primary/hook operator safety FAIL'}
 cmake -S renderer-core/integrated -B build -A x64
 if($LASTEXITCODE -ne 0){throw 'V253 physical cut rematerialization failed'}
 $generated=@(Get-ChildItem build/generated -Recurse -Filter integrated_addon_physical_cut.cpp)
 if($generated.Count -ne 1){throw "V253 cut count mismatch $($generated.Count)"}
 $cut=[IO.File]::ReadAllText($generated[0].FullName)
 foreach($tag in @('v251_lerp_witness','[DSRRL V250 PMETAL LERP DRAW]')){
   if(!$cut.Contains($tag)){throw "V253 physical cut marker missing $tag"}
 }
 $env=[IO.File]::ReadAllText('renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp')
 if(!$env.Contains('[DSRRL V253 PTDE ENVSPEC RGBM]') -or !$env.Contains('ptde_spec_exact')){
    throw 'V253 actual compiled EnvSpec source missing PTDE bank/row replacement'}
 $tx=[IO.File]::ReadAllText('renderer-core/src/runtime/draw_state_transaction.cpp')
 if(!$tx.Contains('[DSRRL V251 LERP NATIVE]')){
     throw 'V253 native bind stage missing in actual compiled source'}
 cmake --build build --config Release --target dsrrl_renderer_core_integrated
 if($LASTEXITCODE -ne 0){throw 'V253 native addon compile failed'}
 cmake --build build/renderer-core-lib --config Release --target dsrrl_pmetal_producer_state_tests
 if($LASTEXITCODE -ne 0){throw 'V253 producer unit compile failed'}
 & 'build/renderer-core-lib/Release/dsrrl_pmetal_producer_state_tests.exe'
 if($LASTEXITCODE -ne 0){throw 'V253 producer native unit FAIL'}
 $cache=[IO.File]::ReadAllText('build/CMakeCache.txt')
 foreach($flag in @(
   'DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE:BOOL=ON',
   'DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW:BOOL=ON',
   'DSRRL_RELEASE_CLEANUP:BOOL=ON')){
   if(!$cache.Contains($flag)){throw "V253 required flag missing: $flag"}
 }
 foreach($flag in @('DSRRL_STUTTER_PROFILE:BOOL=ON',
                    'DSRRL_STUTTER_HITCH_TRACE:BOOL=ON')){
   if($cache.Contains($flag)){throw "V253 unintended profiler: $flag"}
 }
 $base='build/Release/DSRRL_Restored_Lighting_v2_0_2.addon64'
 if(!(Test-Path $base)){throw 'V253 integrated addon missing'}
 $out='DSRRL_V253_EXACT_PTDE_ENVSPEC_RGBM_V252_SAFE.addon64'
 Copy-Item $base $out -Force
 $bytes=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($out))
 foreach($tag in @(
  '[DSRRL V253 PTDE ENVSPEC RGBM]',
  '[DSRRL V252 NONMETAL LERP]',
  '[DSRRL V251 LERP NATIVE]',
  '[DSRRL V250 PMETAL LERP DRAW]',
  '[DSRRL V250 PMETAL LERP PREPARE]',
  '[DSRRL V249 PMETAL SOURCE JOIN]',
  '[DSRRL PMETAL R19 LERP]')){
   if(!$bytes.Contains($tag)){throw "V253 final binary marker missing $tag"}
 }
 $sha=(Get-FileHash $out -Algorithm SHA256).Hash.ToLowerInvariant()
 Write-Host "V253_SINGLE_ADDON_SHA256=$sha"
 Write-Host 'V253_EXACT_PTDE_LIGHTBANK_ENVSPEC_RGBM_CONSTRUCTION_PASS'
 Write-Host 'V253_RUNTIME_RGBM_ACTIVATION_AND_PIXEL=OPEN'
} finally {
 $header='renderer-core/include/dsrrl/runtime/generated_v253_ptde_envspec_rgbm.hpp'
 if(Test-Path $header){Remove-Item $header -Force}
 foreach($path in $paths){[IO.File]::WriteAllText($path,$original[$path])}
 foreach($path in $paths){
  if([IO.File]::ReadAllText($path) -cne $original[$path]){
   throw "V253 tracked source restore mismatch $path"}
 }
}
