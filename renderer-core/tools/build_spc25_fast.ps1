# SPC25 fast, reproducible, incremental Win64 addon builder.
# Usage (from repo root): pwsh -File renderer-core/tools/build_spc25_fast.ps1
# Use -FullTests for all exact SPC25/native C100 tests.
[CmdletBinding()]
param(
    [switch]$FullTests,
    [switch]$Clean,
    [ValidateRange(1, 16)][int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Set-Location $root
$reshade = Join-Path $root '_reshade'
$pinned = '3645e3025d1d98a90e318278858931f034d5d1f6'
if (-not (Test-Path (Join-Path $reshade 'include/reshade.hpp'))) {
    if (Test-Path $reshade) {
        throw 'Existing _reshade is missing include/reshade.hpp; inspect or remove it first'
    }
    git init $reshade
    if ($LASTEXITCODE -ne 0) { throw 'git init failed' }
    git -C $reshade remote add origin https://github.com/crosire/reshade.git
    if ($LASTEXITCODE -ne 0) { throw 'git remote failed' }
    git -C $reshade fetch --depth 1 origin $pinned
    if ($LASTEXITCODE -ne 0) { throw 'Pinned ReShade fetch failed' }
    git -C $reshade checkout --detach FETCH_HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Pinned ReShade checkout failed' }
}
$reshadeHead = (git -C $reshade rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $reshadeHead -ne $pinned) {
    throw "ReShade provenance mismatch: expected $pinned; got $reshadeHead"
}
$commit = (git rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') {
    throw 'Unable to establish exact DSRRL source SHA'
}
$build = Join-Path $root 'build-spc25-fast'
if ($Clean -and (Test-Path $build)) {
    Remove-Item $build -Recurse -Force
}
$flavor = 'v203_spc25_c5330_route14_rx34_t10_test'
$flags = @(
    'DSRRL_RELEASE_CLEANUP',
    'DSRRL_RESOURCE_EPOCH_SHARD_SYNC',
    'DSRRL_COMPANION_TLS_4WAY',
    'DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC',
    'DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH',
    'DSRRL_EXPERIMENTAL_SPC25_C5330_SWAP_TEST',
    'DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG',
    'DSRRL_PMETAL_FULL_PTDE_HEMENV_R3',
    'DSRRL_PMETAL_FULL_PTDE_HEMENV_R4',
    'DSRRL_PMETAL_FULL_PTDE_HEMENV_R5',
    'DSRRL_PMETAL_FULL_PTDE_HEMENV_R6',
    'DSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW',
    'DSRRL_PMETAL_FULL_PTDE_HEMENV_R8_PRE_AB',
    'DSRRL_PMETAL_FULL_PTDE_HEMENV_R11_COMMON_MERGE',
    'DSRRL_PMETAL_FULL_PTDE_HEMENV_R15_PTDE_LIVEOUT_PAIR',
    'DSRRL_PMETAL_FULL_PTDE_HEMENV_R16_LEGACY_DIFFUSE_SPLIT',
    'DSRRL_PMETAL_FULL_PTDE_HEMENV_R17_PTDE_DIFFUSE_MATERIAL',
    'DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW',
    'DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE',
    'DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE',
    'DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL'
)
# Apply the exact fail-open SPC25 lifetime recipe before source verification.
python renderer-core/tools/apply_spc25_native_t1_epoch.py --apply
if ($LASTEXITCODE -ne 0) { throw 'SPC25 native t1 epoch recipe failed' }
python renderer-core/tools/apply_spc25_native_t1_epoch.py
if ($LASTEXITCODE -ne 0) { throw 'SPC25 epoch postcondition failed' }
# The recipe may alter exactly two runtime sources.  A dirty worktree or
# unexpected modification must never be silently packaged as this lineage.
$changedPaths = @(git diff --name-only)
$allowedPaths = @(
    'renderer-core/src/runtime/texture_identity_transport.cpp',
    'renderer-core/src/runtime/material_resource_draw_runtime.cpp',
    'renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp'
)
$unexpectedPaths = @($changedPaths | Where-Object { $_ -notin $allowedPaths })
if ($LASTEXITCODE -ne 0 -or $unexpectedPaths.Count -ne 0) {
    throw "Unexpected SPC25 source mutation: $($unexpectedPaths -join ', ')"
}
foreach ($requiredPath in $allowedPaths) {
    if ($changedPaths -notcontains $requiredPath) {
        throw "Expected SPC25 patched source missing: $requiredPath"
    }
    $digest = (Get-FileHash $requiredPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host "SPC25_PATCHED_SOURCE $requiredPath sha256=$digest"
}
# Isolated native x64 generation/collision regression; fail before touching the addon build.
$epochBuild = Join-Path $root 'build-spc25-native-t1-epoch'
cmake -S renderer-core/tests/spc25_native_t1_epoch -B $epochBuild -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'SPC25 epoch regression configure failed' }
cmake --build $epochBuild --config Release --target spc25_native_t1_epoch_test --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'SPC25 epoch regression native compile failed' }
ctest --test-dir $epochBuild -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'SPC25 epoch regression test failed' }
# Mandatory short source audits run before build, never waive fail-open.
python renderer-core/tools/verify_other_metal_material_workflow_v203.py
if ($LASTEXITCODE -ne 0) { throw 'Exact MTD authority audit failed' }
python renderer-core/tools/verify_spc_batch_v203.py
if ($LASTEXITCODE -ne 0) { throw 'SPC25 static safety audit failed' }
python renderer-core/tools/test_spc25_dsr_tpf_fingerprints.py
if ($LASTEXITCODE -ne 0) { throw 'DDS/TPF identity audit failed' }
$args = @(
    '-S', 'renderer-core/integrated',
    '-B', $build,
    '-G', 'Visual Studio 17 2022',
    '-A', 'x64',
    "-DRESHADE_INCLUDE_DIR=$(Join-Path $reshade 'include')",
    "-DDSRRL_SOURCE_COMMIT=$commit",
    "-DDSRRL_BUILD_FLAVOR=$flavor",
    '-DCMAKE_CXX_FLAGS=/MP /EHsc'
)
foreach ($flag in $flags) { $args += "-D$flag=ON" }
cmake @args
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
cmake --build $build --config Release --target dsrrl_renderer_core_integrated --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'SPC25 addon build failed' }
if ($FullTests) {
    $tests = @(
        'dsrrl_renderer_ptde_metal_envspec_baseline_tests',
        'dsrrl_renderer_ptde_metal_envspec_experimental_tests',
        'dsrrl_renderer_ptde_spc_material_batch_tests',
        'dsrrl_renderer_ptde_workflow_c100_tests',
        'dsrrl_renderer_ptde_metal_exact_source_join_tests'
    )
    cmake --build (Join-Path $build 'renderer-core-lib') --config Release --target $tests --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'SPC25 native tests build failed' }
    ctest --test-dir (Join-Path $build 'renderer-core-lib') -C Release -R '^dsrrl_renderer_ptde_(metal_envspec|spc_material_batch|workflow_c100|metal_exact_source_join)' --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'SPC25 native regression failed' }
}
$addons = @(Get-ChildItem $build -Recurse -Filter '*.addon64' -File)
if ($addons.Count -ne 1) { throw "Expected one integrated addon; found $($addons.Count)" }
$output = Join-Path $root 'DSRRL_v203_other_metal_exact_DIAG.addon64'
Copy-Item $addons[0].FullName $output -Force
$ascii = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($output))
foreach ($needle in @($commit, $flavor, '[DSRRL SPC25 CPU 808D]', 'cpu_808d_writer=%llu', '[DSRRL SPC ONEPASS] stage=spec_reject', 'matching_epoch=%u live_epoch=%llu writer_epoch=%llu')) {
    if (-not $ascii.Contains($needle)) { throw "Binary identity/diagnostic marker missing: $needle" }
}
$sha = (Get-FileHash $output -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "SPC25_FAST_BUILD_PASS commit=$commit sha256=$sha"
Write-Host "ADDON=$output"
Write-Host 'CONSTRUCTION/COMPATIBILITY only; runtime and pixels OPEN'
