param(
    [Parameter(Mandatory=$true)][string]$ReShadeInclude
)
$ErrorActionPreference = 'Stop'
# Exact V350 RC1 renderer features, diagnostic census-only flags excluded.
$env:CL = '/DDSRRL_V344_PTDE_LERP_SPEC_POST_ENV /DDSRRL_V345_UNIFIED_HEMENVLERP /DDSRRL_V346_LERP_REJECT_STAGE /DDSRRL_V347_LERP_ENVDIFFUSE_REBASE /DDSRRL_V348_PTDE_LERP_SPECRGB_RGB_FIX /DDSRRL_V349_PTDE_LERP_ENVSPEC_YZW_FIX /DDSRRL_V350_PTDE_LERP_RGB_COMPONENT_SWIZZLE'
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
if ($LASTEXITCODE -ne 0) { throw "V304 PTDE/DSR source census failed" }
python renderer-core/tools/materialize_v304_typed_dsr_only_donors.py
if ($LASTEXITCODE -ne 0) { throw "V304 typed donor source patch failed" }
python renderer-core/tools/test_v304_typed_donor_contract.py
if ($LASTEXITCODE -ne 0) { throw "V304 typed donor source contract failed" }
python renderer-core/tools/test_v305_dsr_host_spec_route.py
if ($LASTEXITCODE -ne 0) { throw "V305 exact 749/544/2 host-name contract failed" }
python renderer-core/tools/test_v300_mixed_lightbank_donors.py
if ($LASTEXITCODE -ne 0) { throw "V304 donor crosswalk test failed" }
python renderer-core/tools/test_v297_authored_slot0_source.py
if ($LASTEXITCODE -ne 0) { throw "V297 source contract failed" }
python renderer-core/tools/test_v298_live_mtd_source_contract.py
if ($LASTEXITCODE -ne 0) { throw "V298 source contract failed" }
python renderer-core/tools/test_v299_source_consumer_contract.py
if ($LASTEXITCODE -ne 0) { throw "V299 source contract failed" }
python renderer-core/tools/test_v300_leather_atomic_source.py
if ($LASTEXITCODE -ne 0) { throw "V300 source contract failed" }
python renderer-core/tools/materialize_v306_exact_c_leather.py
if ($LASTEXITCODE -ne 0) { throw "V306 C_Leather materializer failed" }
python renderer-core/tools/test_v306_exact_c_leather_source.py
if ($LASTEXITCODE -ne 0) { throw "V306 C_Leather source/consumer contract failed" }
python renderer-core/tools/materialize_v307_exact_c_dullleather.py
if ($LASTEXITCODE -ne 0) { throw "V307 C_DullLeather source/parser construction failed" }
python renderer-core/tools/test_v307_exact_c_dullleather_source.py
if ($LASTEXITCODE -ne 0) { throw "V307 exact source/consumer material contract failed" }
python renderer-core/tools/materialize_v308_exact_six_mtd.py
if ($LASTEXITCODE -ne 0) { throw "V308 six-MTD original source materializer failed" }
python renderer-core/tools/test_v308_exact_source_consumer.py
if ($LASTEXITCODE -ne 0) { throw "V308 source/consumer contract failed" }
python renderer-core/tools/materialize_v309_character_source_compiler.py
if ($LASTEXITCODE -ne 0) { throw "V309 character MTD compiler failed" }
python renderer-core/tools/test_v309_character_source_consumer.py
if ($LASTEXITCODE -ne 0) { throw "V309 source/consumer contract failed" }
python renderer-core/tools/materialize_v310_live_parser_owner_sha_join.py
if ($LASTEXITCODE -ne 0) { throw "V310 live-parser/owner reconciliation materializer failed" }
python renderer-core/tools/test_v310_live_parser_owner_contract.py
if ($LASTEXITCODE -ne 0) { throw "V310 strict parser SHA + owner contract failed" }
python renderer-core/tools/materialize_v311_live_direct_owner.py
if ($LASTEXITCODE -ne 0) { throw "V311 direct original model+live MTD materializer failed" }
python renderer-core/tools/test_v311_owner_recovery_contract.py
if ($LASTEXITCODE -ne 0) { throw "V311 source owner/draw and protect contract failed" }
python renderer-core/tools/materialize_v312_exact_difspcbmp_cohorts.py
if ($LASTEXITCODE -ne 0) { throw "V312 exact PTDE original 53 MTD cohort materializer failed" }
python renderer-core/tools/test_v312_exact_difspcbmp_source_consumer.py
if ($LASTEXITCODE -ne 0) { throw "V312 original bitwise authored 53 MTD source/consumer contract failed" }
python renderer-core/tools/materialize_v313_exact_metal_wet_slot23.py
if ($LASTEXITCODE -ne 0) { throw "V313 exact original PTDE Metal2 Wet3 compiler failed" }
python renderer-core/tools/test_v313_slot23_source_consumer.py
if ($LASTEXITCODE -ne 0) { throw "V313 original PTDE physical slot2/3 contract failed" }
python renderer-core/tools/materialize_v314_original_lit_mul_sources.py
if ($LASTEXITCODE -ne 0) { throw "V314 original 69 SPX source materializer failed" }
python renderer-core/tools/materialize_v314_unrouted_receiver_probe.py
if ($LASTEXITCODE -ne 0) { throw "V314 true PS SHA instrument patch failed" }
python renderer-core/tools/test_v314_lit_mul_receiver_contract.py
if ($LASTEXITCODE -ne 0) { throw "V314 exact original source and stock-only receiver contract failed" }
python renderer-core/tools/materialize_v315_direct_749_runtime_ledger.py
if ($LASTEXITCODE -ne 0) { throw "V315 exact 749 non-low original stock t1 draw ledger materializer failed" }
python renderer-core/tools/test_v315_749_coverage_ledger_contract.py
if ($LASTEXITCODE -ne 0) { throw "V315 per-sidecar real issue/restoration source contract failed" }
python renderer-core/tools/test_v322_corrected_bnd3_named_sha_and_slot_guard.py
if ($LASTEXITCODE -ne 0) { throw 'V322 corrected-BND3 named SHA contract failed' }
python renderer-core/tools/apply_v323_exact_resource_name_srv.py --apply
if ($LASTEXITCODE -ne 0) { throw 'V323 resource bridge patch failed' }
python renderer-core/tools/test_v350_perf_source_contract.py
if ($LASTEXITCODE -ne 0) { throw 'V350 performance source contract failed' }
python renderer-core/tools/test_v350_sync_source_contract.py
if ($LASTEXITCODE -ne 0) { throw 'V350 shared-lock synchronization source contract failed' }
python renderer-core/tools/audit_v350_ptde_rgb_lane_swizzle.py
if ($LASTEXITCODE -ne 0) { throw 'V350 RGB swizzle source contract failed' }
python renderer-core/tools/audit_v338_nonmetal_lerp_else_rgb.py
if ($LASTEXITCODE -ne 0) { throw 'V338 corrected YZW carrier source contract failed' }
python renderer-core/tools/test_v323_exact_resource_transport_contract.py
if ($LASTEXITCODE -ne 0) { throw 'V323 source guard failed' }
$argsList = @(
    '-S','renderer-core/integrated','-B','build-v350-sync','-A','x64',
    "-DRESHADE_INCLUDE_DIR=$ReShadeInclude",
    "-DDSRRL_SOURCE_COMMIT=$commit",
    '-DDSRRL_BUILD_FLAVOR=v350_rc1_sync_shard_read_noqpc',
    '-DCMAKE_CXX_FLAGS=/EHsc /DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID /DDSRRL_V297_AUTHORED_SLOT0_DIAG /DDSRRL_V298_LIVE_MTD_WITNESS /DDSRRL_V299_AUTHORED_SLOT0_OPERATOR /DDSRRL_V300_LEATHER_SLOT1_ATOMIC /DDSRRL_V300_DSR_ONLY_EXACT_DONORS /DDSRRL_V304_TYPED_DSR_ONLY_DONOR /DDSRRL_V305_DSR_DIRECT_SPEC_ROUTE /DDSRRL_V306_EXACT_C_LEATHER /DDSRRL_V307_EXACT_C_DULLLEATHER /DDSRRL_V308_EXACT_MTD_17 /DDSRRL_V309_CHARACTER_MTD_22 /DDSRRL_V310_LIVE_PARSER_OWNER_SHA_JOIN /DDSRRL_V311_DIRECT_MODEL_LIVE_MTD_OWNER /DDSRRL_V312_EXACT_HOMOLOGOUS_53 /DDSRRL_V313_EXACT_METAL2_WET3 /DDSRRL_V314_UNROUTED_LIT_MUL_CENSUS /DDSRRL_V315_DIRECT_749_RUNTIME_LEDGER',
    '-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF',
    '-DDSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG=ON',
    '-DDSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW=ON',
    '-DDSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE=ON',
    '-DDSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE=ON',
    '-DDSRRL_PHYSICAL_CUT_POINTLIGHT_ALL=ON',
    '-DDSRRL_V285_ATOMIC_NONMETAL_DIAG=ON',
    '-DDSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=ON',

    '-DDSRRL_V293_PARSER_EXACT_FLVER_ALIAS=ON',
    '-DDSRRL_FLVER_SELECTOR_PROFILE=OFF',
    '-DDSRRL_POINTLIGHT_PROFILE=OFF'
)

# V338 release gate: fail if the repaired nonmetal RX33-35 shader is not enabled.
if (-not (($argsList -join ' ').Contains('-DDSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=ON'))) { throw "Nonmetal R8 missing" }
foreach ($feature in @('R3','R4','R5','R6','R7_SHADOW','R8_PRE_AB','R11_COMMON_MERGE','R15_PTDE_LIVEOUT_PAIR','R16_LEGACY_DIFFUSE_SPLIT','R17_PTDE_DIFFUSE_MATERIAL')) {
    $argsList += "-DDSRRL_PMETAL_FULL_PTDE_HEMENV_$feature=ON"
}
& cmake @argsList
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
cmake --build build-v350-sync --config Release --target dsrrl_renderer_core_integrated
if ($LASTEXITCODE -ne 0) { throw "One-addon build failed" }
$files = @(Get-ChildItem build-v350-sync -Recurse -Filter *.addon64)
if ($files.Count -ne 1) { throw "Expected one addon, got $($files.Count)" }
New-Item -ItemType Directory -Force out-v350-sync | Out-Null
$dest = 'out-v350-sync/DSRRL_V350_RC1_SYNC_OPTIMIZED.addon64'
Copy-Item $files[0].FullName $dest -Force
$bintext = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes((Resolve-Path $dest)))
if (-not $bintext.Contains($commit) -or -not $bintext.Contains('v350_rc1_sync_shard_read_noqpc') -or -not $bintext.Contains('[DSRRL V350 PTDE LERP RGB]')) {
    throw "Binary provenance/build flavor verification FAILED"
}
(Get-FileHash $dest -Algorithm SHA256) | Format-List
Write-Host 'V322 CORRECT BND3 source rollback CONSTRUCTION ONLY; no invalid slot override; PTDE nonmetal pixel and FPS OPEN'
