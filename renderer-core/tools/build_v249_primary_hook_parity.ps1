# V249: preserve per-material producer TLS across unrelated selectors
# and reconcile exact PMetal source with the owner-proven LightBank hook.
# Build on fully materialized V245, not legacy V229, and restore source.
$ErrorActionPreference = 'Stop'
$paths = @(
    'renderer-core/src/runtime/flver_engine_hooks.cpp',
    'renderer-core/src/runtime/pmetal_env_source_runtime.cpp',
    'renderer-core/src/runtime/pmetal_producer_state.cpp'
)
$sourceDonor = 'bb63009d3d079424162b35a0bc9e791849a90dee'
git diff --quiet $sourceDonor HEAD -- renderer-core/src renderer-core/integrated renderer-core/include
if ($LASTEXITCODE -ne 0) { throw 'V249 runtime donor or protected shader changed' }

./renderer-core/tools/build_v245_exact_source_record_rearm.ps1
if ($LASTEXITCODE -ne 0) { throw 'V249 requires native V245 full-source baseline' }
$original = @{}
foreach ($path in $paths) { $original[$path] = [IO.File]::ReadAllText($path) }

try {
    # V245 restored producer source after its native build. Re-apply exact
    # synchronized V236 and full-payload V245 patches before *incremental*
    # native V249 compile, or MSVC would silently relink an unpatched producer.
    python renderer-core/tools/apply_v236_slot_sync.py --apply
    if ($LASTEXITCODE -ne 0) { throw 'V249 failed to restore V236 mutex' }
    python renderer-core/tools/apply_v245_exact_source_record_rearm.py --apply
    if ($LASTEXITCODE -ne 0) { throw 'V249 failed to restore V245 payload repair' }
    python renderer-core/tools/apply_v245_exact_source_record_rearm.py
    if ($LASTEXITCODE -ne 0) { throw 'V249 V245 producer postcondition failed' }

    python renderer-core/tools/test_v249_primary_source_join.py
    if ($LASTEXITCODE -ne 0) { throw 'V249 generated source oracle failed' }
    python renderer-core/tools/apply_v249_primary_source_join.py --apply
    if ($LASTEXITCODE -ne 0) { throw 'V249 operator-local patch rejected source preimage' }
    python renderer-core/tools/apply_v249_primary_source_join.py
    if ($LASTEXITCODE -ne 0) { throw 'V249 patch postcondition failed' }
    python renderer-core/tools/test_v249_primary_source_join.py
    if ($LASTEXITCODE -ne 0) { throw 'V249 postmaterializer source test failed' }

    # Preserve the V230 DDS hotpath and V231 native COM exception safety.
    $materialSource = [IO.File]::ReadAllText(
        'renderer-core/src/runtime/material_resource_draw_runtime.cpp')
    foreach ($token in @(
        'const std::filesystem::path &process_dir() noexcept',
        'constexpr std::size_t k_release_batch = 64u',
        'g_quarantined.store('
    )) {
        if (!$materialSource.Contains($token)) {
            throw "V249 protected V230/V231 safety overlay missing: $token"
        }
    }
    # The V220/V223 nonmetal path remains (builder has all exact owner flags).
    $envSource = [IO.File]::ReadAllText($paths[1])
    foreach ($token in @(
        '[DSRRL V223 SHARED LIGHTBANK]',
        '[DSRRL V220 SOURCE JOIN]',
        '[DSRRL V249 PMETAL SOURCE JOIN]'
    )) {
        if (!$envSource.Contains($token)) { throw "V249 required hook/nonmetal path missing: $token" }
    }

    cmake --build build --config Release --target dsrrl_renderer_core_integrated
    if ($LASTEXITCODE -ne 0) { throw 'V249 single native Win64 addon compile failed' }

    # Rebuild unit binary against same exact synchronized/payload state
    # actually present in the final addon.
    cmake --build build/renderer-core-lib --config Release --target dsrrl_pmetal_producer_state_tests
    if ($LASTEXITCODE -ne 0) { throw 'V249 synchronized producer native unit compilation failed' }
    & 'build/renderer-core-lib/Release/dsrrl_pmetal_producer_state_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'V249 native producer test FAILED' }

    $cache = [IO.File]::ReadAllText('build/CMakeCache.txt')
    foreach ($flag in @(
        'DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE:BOOL=ON',
        'DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW:BOOL=ON',
        'DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL:BOOL=ON',
        'DSRRL_RELEASE_CLEANUP:BOOL=ON'
    )) { if (!$cache.Contains($flag)) { throw "V249 required operator flag absent: $flag" } }
    foreach ($flag in @('DSRRL_STUTTER_PROFILE:BOOL=ON','DSRRL_STUTTER_HITCH_TRACE:BOOL=ON')) {
        if ($cache.Contains($flag)) { throw "V249 QPC profiler enabled: $flag" }
    }

    $source = 'build/Release/DSRRL_Restored_Lighting_v2_0_2.addon64'
    if (!(Test-Path $source)) { throw 'V249 integrated addon missing' }
    $out = 'DSRRL_V249_PMETAL_PRIMARY_HOOK_PARITY_V245_SAFETY.addon64'
    Copy-Item $source $out -Force
    $binary = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($out))
    foreach ($token in @(
        '[DSRRL V249 PMETAL SOURCE JOIN]',
        '[DSRRL PMETAL R19 LERP]',
        '[DSRRL V223 SHARED LIGHTBANK]'
    )) {
        if (!$binary.Contains($token)) { throw "V249 compiled addon missing $token" }
    }
    $sha = (Get-FileHash $out -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host "V249_SINGLE_ADDON_SHA256=$sha"
    Write-Host 'V249_FULL_V245_SAFETY_AND_PRIMARY_JOIN_BUILD_PASS'
    Write-Host 'V249_PIXEL_BEHAVIOR=OPEN'
} finally {
    foreach ($path in $paths) { [IO.File]::WriteAllText($path, $original[$path]) }
    foreach ($path in $paths) {
        if ([IO.File]::ReadAllText($path) -cne $original[$path]) {
            throw "V249 failed tracked source restoration: $path"
        }
    }
}
