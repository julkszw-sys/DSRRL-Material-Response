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
python renderer-core/tools/materialize_v298_live_mtd_witness.py
if ($LASTEXITCODE -ne 0) { throw "V298 exact parser witness patch failed" }
python renderer-core/tools/materialize_v299_slot0_source_consumer.py
if ($LASTEXITCODE -ne 0) { throw "V299 PTDE source/consumer patch failed" }
python renderer-core/tools/materialize_v299_oct10_candidate_census.py
if ($LASTEXITCODE -ne 0) { throw "V299 census patch failed" }
python renderer-core/tools/materialize_v300_leather_operator.py
if ($LASTEXITCODE -ne 0) { throw "V300 leather operator patch failed" }
python renderer-core/tools/materialize_v300_retail_leather_owner.py
if ($LASTEXITCODE -ne 0) { throw "V300 leather parser patch failed" }
python renderer-core/tools/materialize_v300_leather_census.py
if ($LASTEXITCODE -ne 0) { throw "V300 census patch failed" }
python renderer-core/tools/materialize_v300_donor_provenance.py
if ($LASTEXITCODE -ne 0) { throw "V301 mixed PTDE/DSR source route failed" }
python renderer-core/tools/test_v300_mixed_lightbank_donors.py
if ($LASTEXITCODE -ne 0) { throw "V301 donor crosswalk test failed" }
python renderer-core/tools/test_v297_authored_slot0_source.py
if ($LASTEXITCODE -ne 0) { throw "V297 source contract failed" }
python renderer-core/tools/test_v298_live_mtd_source_contract.py
if ($LASTEXITCODE -ne 0) { throw "V298 source contract failed" }
python renderer-core/tools/test_v299_source_consumer_contract.py
if ($LASTEXITCODE -ne 0) { throw "V299 source contract failed" }
python renderer-core/tools/test_v300_leather_atomic_source.py
if ($LASTEXITCODE -ne 0) { throw "V300 source contract failed" }
$argsList = @(
    '-S','renderer-core/integrated','-B','build-v301','-A','x64',
    "-DRESHADE_INCLUDE_DIR=$ReShadeInclude",
    "-DDSRRL_SOURCE_COMMIT=$commit",
    '-DDSRRL_BUILD_FLAVOR=v301_mixed_origin_slot0_slot1_atomic',
    '-DCMAKE_CXX_FLAGS=/EHsc /DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID /DDSRRL_V297_AUTHORED_SLOT0_DIAG /DDSRRL_V298_LIVE_MTD_WITNESS /DDSRRL_V299_AUTHORED_SLOT0_OPERATOR /DDSRRL_V300_LEATHER_SLOT1_ATOMIC /DDSRRL_V300_DSR_ONLY_EXACT_DONORS',
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
cmake --build build-v301 --config Release --target dsrrl_renderer_core_integrated
if ($LASTEXITCODE -ne 0) { throw "One-addon build failed" }
$files = @(Get-ChildItem build-v301 -Recurse -Filter *.addon64)
if ($files.Count -ne 1) { throw "Expected one addon, got $($files.Count)" }
New-Item -ItemType Directory -Force out-v301 | Out-Null
$dest = 'out-v301/DSRRL_V301_PTDE_DSR_MIXED_DONOR_ATOMIC.addon64'
Copy-Item $files[0].FullName $dest -Force
$bintext = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes((Resolve-Path $dest)))
if (-not $bintext.Contains($commit) -or -not $bintext.Contains('v301_mixed_origin_slot0_slot1_atomic')) {
    throw "Binary provenance/build flavor verification FAILED"
}
(Get-FileHash $dest -Algorithm SHA256) | Format-List
Write-Host 'V301 NATIVE CONSTRUCTION PASS only; runtime and PTDE pixels NOT VERIFIED'
