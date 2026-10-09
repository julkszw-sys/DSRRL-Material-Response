$ErrorActionPreference = "Stop"
$commit = (git rev-parse HEAD).Trim()
python renderer-core/tools/materialize_v202_production_zero_hybrid.py
if ($LASTEXITCODE -ne 0) { throw "Failed original DSR donor or native source verification" }
python renderer-core/tools/audit_r43_release_cleanup_hardening.py
if ($LASTEXITCODE -ne 0) { throw "Production source audit failed" }
$flags = @(
"-DRESHADE_INCLUDE_DIR=$pwd/_reshade/include",
"-DDSRRL_SOURCE_COMMIT=$commit",
"-DDSRRL_BUILD_FLAVOR=v2_0_2_production_zero_dsr_only_hybrid",
"-DDSRRL_RELEASE_CLEANUP=ON",
"-DDSRRL_RESOURCE_EPOCH_SHARD_SYNC=ON",
"-DDSRRL_COMPANION_TLS_4WAY=ON",
"-DDSRRL_FULL_TELEMETRY_DEFAULT_ON=OFF",
"-DDSRRL_STUTTER_PROFILE=OFF",
"-DDSRRL_STUTTER_HITCH_TRACE=OFF",
"-DDSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG=ON",
"-DDSRRL_PMETAL_FULL_PTDE_HEMENV_R3=ON",
"-DDSRRL_PMETAL_FULL_PTDE_HEMENV_R4=ON",
"-DDSRRL_PMETAL_FULL_PTDE_HEMENV_R5=ON",
"-DDSRRL_PMETAL_FULL_PTDE_HEMENV_R6=ON",
"-DDSRRL_PMETAL_FULL_PTDE_HEMENV_R7_SHADOW=ON",
"-DDSRRL_PMETAL_FULL_PTDE_HEMENV_R8_PRE_AB=ON",
"-DDSRRL_PMETAL_FULL_PTDE_HEMENV_R11_COMMON_MERGE=ON",
"-DDSRRL_PMETAL_FULL_PTDE_HEMENV_R15_PTDE_LIVEOUT_PAIR=ON",
"-DDSRRL_PMETAL_FULL_PTDE_HEMENV_R16_LEGACY_DIFFUSE_SPLIT=ON",
"-DDSRRL_PMETAL_FULL_PTDE_HEMENV_R17_PTDE_DIFFUSE_MATERIAL=ON",
"-DDSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW=ON",
"-DDSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE=ON",
"-DDSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE=ON",
"-DDSRRL_PHYSICAL_CUT_POINTLIGHT_ALL=ON")
cmake -S renderer-core/integrated -B build -A x64 @flags
if ($LASTEXITCODE -ne 0) { throw "CMake production config failed" }
cmake --build build --config Release --target dsrrl_renderer_core_integrated
if ($LASTEXITCODE -ne 0) { throw "CMake addon build failed" }
