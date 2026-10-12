# V235: narrow synchronization A/B. Inherit bit-exact V233 renderer/receiver
# construction; opt in only to a selector-side per-set mutex wait after
# all source-generation steps complete. Never mutate committed runtime sources.
$ErrorActionPreference = 'Stop'
$src = 'renderer-core/src/runtime/pmetal_env_source_runtime.cpp'
$patch = 'renderer-core/tools/apply_v235_exact_selector_mutex.py'
python renderer-core/tools/test_v235_exact_selector_mutex.py
if ($LASTEXITCODE -ne 0) { throw 'V235 source oracle failed' }
./renderer-core/tools/build_v233_transition_stutter_diagnostic.ps1
if ($LASTEXITCODE -ne 0) { throw 'V235 inherited profiled native build failed' }

# The V233 build recipe materializes V231 renderer C++ in the working tree.
# Keep its generated source bytes for complete exception-safe restoration.
$generated = [IO.File]::ReadAllText((Join-Path (Get-Location) $src))
try {
    python $patch --apply
    if ($LASTEXITCODE -ne 0) { throw 'V235 patch provenance failed' }
    python $patch
    if ($LASTEXITCODE -ne 0) { throw 'V235 patched code verification failed' }
    cmake --build build --config Release --target dsrrl_renderer_core_integrated
    if ($LASTEXITCODE -ne 0) { throw 'V235 native addon incremental compile failed' }
    ./renderer-core/tools/verify_v203_clean_equipment.ps1
    if ($LASTEXITCODE -ne 0) { throw 'V235 native verifier failed' }

    $addon = 'DSRRL_v203_CLEAN_EQUIPMENT_SPEC_EXPERIMENT.addon64'
    if (!(Test-Path $addon)) { throw 'V235 single addon missing' }
    $ascii = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($addon))
    if (!$ascii.Contains('[DSRRL STUTTER R43]') -or
        !$ascii.Contains('[DSRRL HITCH FRAME]')) {
        throw 'V235 full V233 QPC instrumentation missing'
    }
    $dest = 'DSRRL_V235_EXACT_SELECTOR_MUTEX_DIAGNOSTIC.addon64'
    Copy-Item -LiteralPath $addon -Destination $dest -Force
    Write-Host 'V235_SINGLE_ADDON_WITH_V233_PROFILER_PASS'
    Write-Host 'V235_CONSTRUCTION_ONLY selector_mutex_wait=ON producer_trylock=STOCK'
} finally {
    [IO.File]::WriteAllText((Join-Path (Get-Location) $src),$generated)
    if (([IO.File]::ReadAllText((Join-Path (Get-Location) $src))) -cne $generated) {
        throw 'V235 failed to restore generated renderer source'
    }
}
