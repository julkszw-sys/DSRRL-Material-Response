param(
    [Parameter(Mandatory=$true)][string]$ReShadeInclude
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Set-Location $root
$commit = (git rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw "Git source commit unavailable" }
python renderer-core/tools/materialize_v296_oct10_atomic_policy.py
if ($LASTEXITCODE -ne 0) { throw "Source guard rejected; not building" }
python renderer-core/tools/materialize_v297_authored_slot0.py
if ($LASTEXITCODE -ne 0) { throw "V297 source drift; not building" }
python renderer-core/tools/materialize_v297_retail_mtd_owner.py
if ($LASTEXITCODE -ne 0) { throw "V297 retail MTD cache bridge construction failed" }
$argsList = @(
    '-S','renderer-core/integrated','-B','build-v297','-A','x64',
    "-DRESHADE_INCLUDE_DIR=$ReShadeInclude",
    "-DDSRRL_SOURCE_COMMIT=$commit",
    '-DDSRRL_BUILD_FLAVOR=v297_ptde_authored_slot0_atomic',
    '-DCMAKE_CXX_FLAGS=/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID /DDSRRL_V297_AUTHORED_SLOT0_DIAG',
    '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF',
    '-DDSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG=ON',
    '-DDSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW=ON',
    '-DDSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE=ON',
    '-DDSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE=ON',
    '-DDSRRL_PHYSICAL_CUT_POINTLIGHT_ALL=ON',
    '-DDSRRL_V285_ATOMIC_NONMETAL_DIAG=ON',
    '-DDSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=ON',
    '-DDSRRL_V290_FULL_SIDECAR_DRAW_CENSUS=ON',
    '-DDSRRL_V291_MATERIAL_STAGE_CENSUS=ON',
    '-DDSRRL_V291_LERP_CUBEMAP_CENSUS=ON',
    '-DDSRRL_V293_PARSER_EXACT_FLVER_ALIAS=ON'
)
foreach ($feature in @('R3','R4','R5','R6','R7_SHADOW','R8_PRE_AB','R11_COMMON_MERGE','R15_PTDE_LIVEOUT_PAIR','R16_LEGACY_DIFFUSE_SPLIT','R17_PTDE_DIFFUSE_MATERIAL')) {
    $argsList += "-DDSRRL_PMETAL_FULL_PTDE_HEMENV_$feature=ON"
}
& cmake @argsList
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
cmake --build build-v297 --config Release --target dsrrl_renderer_core_integrated
if ($LASTEXITCODE -ne 0) { throw "One-addon build failed" }
$files = @(Get-ChildItem build-v297 -Recurse -Filter *.addon64)
if ($files.Count -ne 1) { throw "Expected one addon, got $($files.Count)" }
New-Item -ItemType Directory -Force out-v297 | Out-Null
$dest = 'out-v297/DSRRL_V297_PTDE_SLOT0_ATOMIC_DIAG.addon64'
Copy-Item $files[0].FullName $dest -Force
$bintext = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes((Resolve-Path $dest)))
if (-not $bintext.Contains($commit) -or -not $bintext.Contains('v297_ptde_authored_slot0_atomic')) {
    throw "Binary provenance/build flavor verification FAILED"
}
(Get-FileHash $dest -Algorithm SHA256) | Format-List
Write-Host 'V297 CONSTRUCTION PASS only; runtime and PTDE pixels NOT VERIFIED'
