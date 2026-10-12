$ErrorActionPreference = "Stop"
$commit = (git rev-parse HEAD).Trim()
git merge-base --is-ancestor 6a73033f6275f88487effeb1e34bbbdd7a6a3a88 HEAD
if ($LASTEXITCODE -ne 0) { throw "Incorrect v203 base" }
python renderer-core/tools/materialize_v202_production_zero_hybrid.py
if ($LASTEXITCODE -ne 0) { throw "Failed original DSR donor or native source verification" }
python renderer-core/tools/audit_r43_release_cleanup_hardening.py
if ($LASTEXITCODE -ne 0) { throw "Production source audit failed" }
if ($env:DSRRL_V203_EQUIPMENT_ENVSPEC36 -eq "ON") {
    python renderer-core/tools/apply_v203_envspec36_generated_source.py --apply
    if ($LASTEXITCODE -ne 0) { throw "Pinned v203 diagnostic source overlay failed" }
    python renderer-core/tools/apply_v203_envspec36_generated_source.py
    if ($LASTEXITCODE -ne 0) { throw "v203 diagnostic source overlay drift" }
}
python renderer-core/tools/test_v203_clean_equipment_manifest.py
if ($LASTEXITCODE -ne 0) { throw "Equipment provenance regression failed" }
python renderer-core/tools/apply_v203_clean_equipment_spec.py --apply
if ($LASTEXITCODE -ne 0) { throw "Equipment overlay failed" }
python renderer-core/tools/apply_v203_clean_equipment_spec.py
if ($LASTEXITCODE -ne 0) { throw "Equipment verification failed" }
if ($env:DSRRL_V203_FULL_SIDECAR_SPECULAR -eq "ON") {
    foreach ($name in @("DSRRL_V203_EQUIPMENT_ENVSPEC36","DSRRL_V203_CONSTEXPR_OWNER_LOOKUP","DSRRL_V203_EXACT_LEATHER_MTD_INDEX","DSRRL_V203_OWNER_PATH_TRACE")) {
        if ([Environment]::GetEnvironmentVariable($name) -ne "ON") {
            throw "Full sidecar Specular whitelist requires pinned PR304 lineage option: $name"
        }
    }
    python renderer-core/tools/apply_v203_full_sidecar_1295.py --apply
    if ($LASTEXITCODE -ne 0) { throw "Full 1295-sidecar routing overlay failed" }
    python renderer-core/tools/apply_v203_full_sidecar_1295.py
    if ($LASTEXITCODE -ne 0) { throw "Full 1295-sidecar routing overlay drift" }
    python renderer-core/tools/apply_v204_sidecar_resource_and_source_fix.py --apply
    if ($LASTEXITCODE -ne 0) { throw "V204 full-resource registration and keyed source overlay failed" }
    python renderer-core/tools/apply_v204_sidecar_resource_and_source_fix.py
    if ($LASTEXITCODE -ne 0) { throw "V204 resource/source overlay drift" }
    if ($env:DSRRL_V206_NATIVE_T1_ADAPTER -eq "ON") {
        python renderer-core/tools/apply_v206_native_t1_adapter.py --apply
        if ($LASTEXITCODE -ne 0) { throw "V206 narrow V204-native adapter failed" }
        python renderer-core/tools/apply_v206_native_t1_adapter.py
        if ($LASTEXITCODE -ne 0) { throw "V206 adapter postcondition failed" }
    }
    if ($env:DSRRL_V214_EXACT_RESOURCE_NAME_SRV -eq "ON") {
        python renderer-core/tools/apply_v214_exact_resource_name_srv.py --apply
        if ($LASTEXITCODE -ne 0) { throw "V214 exact resource name to SRV bridge failed" }
        python renderer-core/tools/apply_v214_exact_resource_name_srv.py
        if ($LASTEXITCODE -ne 0) { throw "V214 exact producer-to-SRV source drift" }
    }
    if ($env:DSRRL_V216_BOUND_T1_NATIVE_ALIAS -eq "ON") {
        if ($env:DSRRL_V214_EXACT_RESOURCE_NAME_SRV -ne "ON") {
            throw "V216 live native t1 alias requires V214 source provenance"
        }
        python renderer-core/tools/apply_v216_bound_t1_native_alias.py --apply
        if ($LASTEXITCODE -ne 0) { throw "V216 actual bound t1 resource alias failed" }
        python renderer-core/tools/apply_v216_bound_t1_native_alias.py
        if ($LASTEXITCODE -ne 0) { throw "V216 bound t1 bridge drift" }
    }
}
if ($env:DSRRL_V220_NONMETAL_SOURCE_JOIN -eq "ON") {
    if ($env:DSRRL_V203_FULL_SIDECAR_SPECULAR -ne "ON" -or
        $env:DSRRL_V203_EQUIPMENT_ENVSPEC36 -ne "ON") {
        throw "V220 requires exact V203 nonmetal sidecar and material owner"
    }
}
if ($env:DSRRL_V219_EXACT_NONMETAL_GATE_TRACE -eq "ON") {
    if ($env:DSRRL_V203_EQUIPMENT_ENVSPEC36 -ne "ON" -or
        $env:DSRRL_V203_FULL_SIDECAR_SPECULAR -ne "ON") {
        throw "V219 draw gating trace requires exact nonmetal shader/sidecar build"
    }
    python renderer-core/tools/apply_v219_exact_nonmetal_first_gate.py --apply
    if ($LASTEXITCODE -ne 0) { throw "V219 exact draw cut overlay failed" }
    python renderer-core/tools/apply_v219_exact_nonmetal_first_gate.py
    if ($LASTEXITCODE -ne 0) { throw "V219 exact draw cut postcondition failed" }
}
if ($env:DSRRL_V220_NONMETAL_SOURCE_JOIN -eq "ON") {
    python renderer-core/tools/test_v220_nonmetal_source_identity_join.py
    if ($LASTEXITCODE -ne 0) { throw "V220 source key contract failed" }
    python renderer-core/tools/apply_v220_nonmetal_source_identity_join.py --apply
    if ($LASTEXITCODE -ne 0) { throw "V220 generated exact nonmetal source join failed" }
    python renderer-core/tools/apply_v220_nonmetal_source_identity_join.py
    if ($LASTEXITCODE -ne 0) { throw "V220 source postcondition failed" }
}
if ($env:DSRRL_V221_NONMETAL_PROBE_CONTEXT_RETRY -eq "ON") {
    foreach ($name in @("DSRRL_V203_EQUIPMENT_ENVSPEC36",
                       "DSRRL_V203_FULL_SIDECAR_SPECULAR",
                       "DSRRL_V219_EXACT_NONMETAL_GATE_TRACE",
                       "DSRRL_V220_NONMETAL_SOURCE_JOIN")) {
        if ([Environment]::GetEnvironmentVariable($name) -ne "ON") {
            throw "V221 requires exact V219/V220 source and EnvSpec sidecar: $name"
        }
    }
    python renderer-core/tools/test_v221_probe_context_retry.py
    if ($LASTEXITCODE -ne 0) { throw "V221 nonmetal probe regression oracle failed" }
    python renderer-core/tools/apply_v221_exact_nonmetal_probe_context_retry.py --apply
    if ($LASTEXITCODE -ne 0) { throw "V221 resource transport overlay failed" }
    python renderer-core/tools/apply_v221_exact_nonmetal_probe_context_retry.py
    if ($LASTEXITCODE -ne 0) { throw "V221 resource transport overlay postcondition failed" }
}
$flags = @(
"-DRESHADE_INCLUDE_DIR=$pwd/_reshade/include",
"-DDSRRL_SOURCE_COMMIT=$commit",
"-DDSRRL_BUILD_FLAVOR=v203_clean_equipment_slot_spec_optin",
"-DDSRRL_RELEASE_CLEANUP=ON",
"-DCMAKE_CXX_FLAGS=/DDSRRL_V203_EXACT_EQUIPMENT_SPEC /EHsc",
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
if ($env:DSRRL_V203_EQUIPMENT_ENVSPEC36 -eq "ON") {
    $flags += "-DDSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG=ON"
    Write-Host "DSRRL: exact 36-owner experimental EnvSpec route enabled; pixel=OPEN"
}
if ($env:DSRRL_V203_CONSTEXPR_OWNER_LOOKUP -eq "ON") {
    if ($env:DSRRL_V203_EQUIPMENT_ENVSPEC36 -ne "ON") {
        throw "Constexpr owner lookup requires EnvSpec36 opt-in"
    }
    $flags += "-DDSRRL_V203_CONSTEXPR_OWNER_LOOKUP=ON"
    Write-Host "DSRRL: exact full-SHA constexpr owner lookup EXPERIMENTAL"
}
if ($env:DSRRL_V203_EXACT_LEATHER_MTD_INDEX -eq "ON") {
    if ($env:DSRRL_V203_EQUIPMENT_ENVSPEC36 -ne "ON" -or
        $env:DSRRL_V203_CONSTEXPR_OWNER_LOOKUP -ne "ON") {
        throw "Raw leather MTD index requires both isolated EnvSpec36 and full-SHA constexpr owner"
    }
    $flags += "-DDSRRL_V203_EXACT_LEATHER_MTD_INDEX=ON"
    Write-Host "DSRRL: experimental three-record exact raw MTD index, no new authority"
}
if ($env:DSRRL_V203_OWNER_PATH_TRACE -eq "ON") {
    if ($env:DSRRL_V203_EQUIPMENT_ENVSPEC36 -ne "ON" -or
        $env:DSRRL_V203_CONSTEXPR_OWNER_LOOKUP -ne "ON" -or
        $env:DSRRL_V203_EXACT_LEATHER_MTD_INDEX -ne "ON") {
        throw "Owner trace requires the exact v203 three-feature opt-in lineage"
    }
    $flags += "-DDSRRL_V203_OWNER_PATH_RUNTIME_DIAG=ON"
    Write-Host "DSRRL: bounded 12-event-per-family owner route trace enabled"
}
if ($env:DSRRL_V203_FULL_SIDECAR_SPECULAR -eq "ON") {
    $flags += "-DDSRRL_V203_FULL_SIDECAR_1295=ON"
    Write-Host "DSRRL: all 1295 Specular sidecar resources whitelisted; exact full-owner+MTD semantics for nonmetal only; P_Metal unchanged"
}
if ($env:DSRRL_V214_EXACT_RESOURCE_NAME_SRV -eq "ON") {
    if ($env:DSRRL_V203_FULL_SIDECAR_SPECULAR -ne "ON" -or
        $env:DSRRL_V206_NATIVE_T1_ADAPTER -ne "ON") {
        throw "V214 exact transport requires full 1295 sidecar and V206 native lifetime bridge"
    }
    $flags += "-DDSRRL_V214_EXACT_RESOURCE_NAME_SRV=ON"
    Write-Host "DSRRL V214: full 1295 loader and exact init_resource to SRV provenance; consumer gates unchanged"
}
if ($env:DSRRL_V216_BOUND_T1_NATIVE_ALIAS -eq "ON") {
    if ($env:DSRRL_V214_EXACT_RESOURCE_NAME_SRV -ne "ON") {
        throw "V216 requires V214 exact identity bridge"
    }
    $flags += "-DDSRRL_V216_BOUND_T1_NATIVE_ALIAS=ON"
    Write-Host "DSRRL V216: draw-bound t1 alternate view of EXACT native resource; PTDE consumer not promoted"
}
if ($env:DSRRL_V219_EXACT_NONMETAL_GATE_TRACE -eq "ON") {
    $flags += "-DDSRRL_V219_EXACT_NONMETAL_GATE_TRACE=ON"
    Write-Host "DSRRL V219: bounded first exact nonmetal draw and operator failure stage; no shader or resource mutation"
}
if ($env:DSRRL_V220_NONMETAL_SOURCE_JOIN -eq "ON") {
    $flags += "-DDSRRL_V220_NONMETAL_SOURCE_JOIN=ON"
    Write-Host "DSRRL V220: authenticated nonmetal source key transport; P_Metal R43 unchanged; pixel=OPEN"
}
if ($env:DSRRL_V221_NONMETAL_PROBE_CONTEXT_RETRY -eq "ON") {
    $flags += "-DDSRRL_V221_NONMETAL_PROBE_CONTEXT_RETRY=ON"
    Write-Host "DSRRL V221: exact nonmetal probe live-context retry, preserves full SHA probe lookup and P_Metal"
}
if ($env:DSRRL_V206_NATIVE_T1_ADAPTER -eq "ON") {
    if ($env:DSRRL_V203_FULL_SIDECAR_SPECULAR -ne "ON") {
        throw "V206 requires V204 full sidecar source and fail-open lineage"
    }
    $flags += "-DDSRRL_V206_SPC25_EQUIPMENT_ID=ON"
    Write-Host "DSRRL V206: minimal native writer + SRV/epoch adapter; NO SPC25 code transplant, no NPC c5330"
}
cmake -S renderer-core/integrated -B build -A x64 @flags
if ($LASTEXITCODE -ne 0) { throw "CMake production config failed" }
cmake --build build --config Release --target dsrrl_renderer_core_integrated
if ($LASTEXITCODE -ne 0) { throw "CMake addon build failed" }
