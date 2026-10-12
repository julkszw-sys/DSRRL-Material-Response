# V247 — restore the last owner-observed visually working P_Metal HemEnvLerp
# build construction (V229), not a shader guess, broad DSR rollback, or
# native cache retrofit. Preserve V203...V224 authenticated nonmetal bridge.
# Keep R43 active untouched. No new owner captures/tests are prerequisites.
$ErrorActionPreference = 'Stop'
$baseline = '75ae33d71d6859ae21d7004b47b740f0a3cb2727'
$tracked = @(
    'renderer-core/src',
    'renderer-core/integrated',
    'renderer-core/tools/build_v229_gloss_witness.ps1',
    'renderer-core/tools/configure_v203_clean_equipment.ps1',
    'renderer-core/tools/apply_v227_lerp_pmetal_nonmetal_parity.py',
    'renderer-core/tools/apply_v229_gloss_material_consumer_witness.py'
)
# Reconstruct prior certified carrier from unchanged tracked ingredients.
git diff --quiet $baseline HEAD -- $tracked
if ($LASTEXITCODE -ne 0) {
    throw 'V247 abort: verified V229 native source, exact scripts or integrated donor drifted'
}
git diff --quiet -- renderer-core
if ($LASTEXITCODE -ne 0) {
    throw 'V247 requires pristine tracked renderer-core sources to avoid changing local work'
}
if (Test-Path 'build/CMakeCache.txt') {
    throw 'V247 needs an isolated pristine native build directory (build/CMakeCache.txt exists)'
}

$mandatory = @(
    'DSRRL_V203_EQUIPMENT_ENVSPEC36',
    'DSRRL_V203_CONSTEXPR_OWNER_LOOKUP',
    'DSRRL_V203_EXACT_LEATHER_MTD_INDEX',
    'DSRRL_V203_OWNER_PATH_TRACE',
    'DSRRL_V203_FULL_SIDECAR_SPECULAR',
    'DSRRL_V206_NATIVE_T1_ADAPTER',
    'DSRRL_V214_EXACT_RESOURCE_NAME_SRV',
    'DSRRL_V216_BOUND_T1_NATIVE_ALIAS',
    'DSRRL_V219_EXACT_NONMETAL_GATE_TRACE',
    'DSRRL_V220_NONMETAL_SOURCE_JOIN',
    'DSRRL_V221_NONMETAL_PROBE_CONTEXT_RETRY',
    'DSRRL_V222_EXACT_PROBE_REJECT_FRONTIER',
    'DSRRL_V223_EXACT_NONMETAL_SHARED_LIGHTBANK_FALLBACK',
    'DSRRL_V224_EXACT_NATIVE_T12_RESOURCE_JOIN'
)
foreach ($name in $mandatory) {
    if ([Environment]::GetEnvironmentVariable($name) -ne 'ON') {
        throw "V247 exact V229 build flag missing: $name"
    }
}
try {
    ./renderer-core/tools/build_v229_gloss_witness.ps1
    if ($LASTEXITCODE -ne 0) { throw 'V247 V229 materialization/Win64 native build failed' }

    python renderer-core/tools/apply_v227_lerp_pmetal_nonmetal_parity.py
    if ($LASTEXITCODE -ne 0) { throw 'V247 P_Metal V227 parity drift' }
    python renderer-core/tools/apply_v229_gloss_material_consumer_witness.py
    if ($LASTEXITCODE -ne 0) { throw 'V247 V229 materialization drift' }

    # Verify actual generated physical-cut, not the merely tracked donor.
    $physical = @(Get-ChildItem 'build/generated' -Recurse -Filter 'integrated_addon_physical_cut.cpp')
    if ($physical.Count -ne 1) { throw "V247 compiled physical-cut count=$($physical.Count)" }
    $actual = [IO.File]::ReadAllText($physical[0].FullName)
    if (!$actual.Contains('k_pmetal_direct_current_native_dispatch')) {
        throw 'V247 direct native P_Metal draw dispatch missing'
    }
    if (!$actual.Contains('[DSRRL V219 MATERIAL DRAW]')) {
        throw 'V247 expected nonmetal-only V219 authority witness missing'
    }
    if (!$actual.Contains('dsrrl::runtime::pmetal_env_source_runtime')) {
        throw 'V247 exact P_Metal source missing from physical-cut'
    }

    # Existing five native tests and one final-addon output contract.
    ./renderer-core/tools/verify_v203_clean_equipment.ps1
    if ($LASTEXITCODE -ne 0) { throw 'V247 exact V229 native regression suite failed' }

    $cache = [IO.File]::ReadAllText('build/CMakeCache.txt')
    foreach ($option in @(
        'DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG',
        'DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE',
        'DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW',
        'DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE',
        'DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL'
    )) {
        if (!$cache.Contains($option + ':BOOL=ON')) {
            throw "V247 required exact PTDE operator option disabled: $option"
        }
    }
    foreach ($notCompiled in @(
        'DSRRL_STUTTER_PROFILE:BOOL=ON',
        'DSRRL_STUTTER_HITCH_TRACE:BOOL=ON'
    )) {
        if ($cache.Contains($notCompiled)) { throw "V247 profiler unexpectedly enabled: $notCompiled" }
    }

    $native = @(Get-ChildItem 'build' -Recurse -Filter '*.addon64')
    if ($native.Count -ne 1) { throw "V247 expected one integrated addon; got $($native.Count)" }
    $exe = 'DSRRL_V247_PMETAL_LERP_V229_RESTORED_V224_NONMETAL.addon64'
    Copy-Item -LiteralPath $native[0].FullName -Destination $exe -Force
    $bytes = [IO.File]::ReadAllBytes($exe)
    $ascii = [Text.Encoding]::ASCII.GetString($bytes)
    foreach ($token in @(
        '[DSRRL PMETAL R19 LERP]',
        '[DSRRL V223 SHARED LIGHTBANK]',
        '[DSRRL V227 LERP PARITY]',
        '[DSRRL V229 GLOSS CUT]',
        'v203_clean_equipment_slot_spec_optin'
    )) {
        if (!$ascii.Contains($token)) { throw "V247 operator/source lineage marker missing: $token" }
    }
    if ($ascii.Contains('[DSRRL HITCH FRAME]')) { throw 'V247 stale QPC diagnostic compiled' }
    $sha = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host "V247_SINGLE_ADDON_SHA256=$sha"
    Write-Host 'V247_CONSTRUCTION_V229_RESTORATION_PASS'
    Write-Host 'V247_PMETAL_OWNER_V229_HISTORICALLY_WORKED_PIXEL_V247_OPEN'
} finally {
    # V229's original builder materializes temporary tracked source directly.
    # Initial pristine gate makes restoring exact HEAD content safe.
    git restore --worktree -- renderer-core
    if ($LASTEXITCODE -ne 0) { throw 'V247 tracked source restore failed' }
    git diff --quiet -- renderer-core
    if ($LASTEXITCODE -ne 0) { throw 'V247 tracked source restore postcondition failed' }
}
