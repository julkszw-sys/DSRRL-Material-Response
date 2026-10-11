$ErrorActionPreference = "Stop"
$sha = (git rev-parse HEAD).Trim()
$native=@("dsrrl_pmetal_selector_policy_tests","dsrrl_pmetal_producer_state_tests","dsrrl_renderer_flver_identity_tests","dsrrl_renderer_resource_view_epoch_tests","dsrrl_renderer_companion_tls_way_tests")
cmake --build build/renderer-core-lib --config Release --target $native
if ($LASTEXITCODE -ne 0) { throw "Native test build failed" }
ctest --test-dir build/renderer-core-lib -C Release -R "^dsrrl_(pmetal_(selector_policy|producer_state)|renderer_(flver_identity|resource_view_epoch|companion_tls_way))_tests$" --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Native tests failed" }
if ($env:DSRRL_V214_EXACT_RESOURCE_NAME_SRV -eq "ON") {
    # The final V214 source is a strict, verified extension of the already
    # verified V206 intermediate stage in configure_v203_clean_equipment.ps1.
    python renderer-core/tools/apply_v214_exact_resource_name_srv.py
    if ($LASTEXITCODE -ne 0) { throw "V214 final exact resource-SRV postcondition drift" }
} elseif ($env:DSRRL_V206_NATIVE_T1_ADAPTER -eq "ON") {
    python renderer-core/tools/apply_v206_native_t1_adapter.py
    if ($LASTEXITCODE -ne 0) { throw "V206 exact V204-native transport provenance adapter source drift" }
} elseif ($env:DSRRL_V203_FULL_SIDECAR_SPECULAR -eq "ON") {
    # PR305's verifier must run BEFORE applying V204. It cannot
    # validate the intentional extended create-time SRV registration.
    # configure_v203_clean_equipment.ps1 already verified the PR305
    # upstream postcondition before the V204 overlay was applied.
    python renderer-core/tools/apply_v204_sidecar_resource_and_source_fix.py
    if ($LASTEXITCODE -ne 0) { throw "V204 SRV init and keyed nonmetal LightBank postcondition failed" }
} else {
    python renderer-core/tools/apply_v203_clean_equipment_spec.py
    if ($LASTEXITCODE -ne 0) { throw "Equipment source postcondition failed" }
}
$files = @(Get-ChildItem build -Recurse -Filter *.addon64)
if ($files.Count -ne 1) { throw "Expected one addon" }
$bin = "DSRRL_v203_CLEAN_EQUIPMENT_SPEC_EXPERIMENT.addon64"
Copy-Item $files[0].FullName $bin
$ascii = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($bin))
foreach ($marker in @($sha,"[DSRRL PMETAL ZERO RE]","v203_clean_equipment_slot_spec_optin","epochs=KEY_BUCKET_256","mode=4WAY_64SETS_256TOTAL_SYNC_ON","[DSRRL PHYSICAL CUT POINTLIGHT]")) {
  if (!$ascii.Contains($marker)) { throw "Missing $marker" }
}
if ($env:DSRRL_V203_FULL_SIDECAR_SPECULAR -eq "ON") {
    foreach ($required in @("[DSRRL SIDECAR 1295]","nonmetal_resource_probe","stage=resource_view_init","ps012=")) {
        if (!$ascii.Contains($required)) { throw "Missing full sidecar 1295 binary diagnostic $required" }
    }
    $cache = Get-Content 'build/CMakeCache.txt' -Raw
    if (!$cache.Contains('DSRRL_V203_FULL_SIDECAR_1295:BOOL=ON')) { throw "Full sidecar compile option disabled" }
}
if ($env:DSRRL_V206_NATIVE_T1_ADAPTER -eq "ON") {
    foreach ($marker in @("[DSRRL V206 CPU WRITER]","[DSRRL V208 NATIVE T1]","v208_lane=","companion_present=","[DSRRL V208 CPU NAME]","[DSRRL V209 ENGINE CLASS]","equipment_syntax=","expected_vtable_rva=14AA4A8","engine_cpu_srv_exact=","no-swap-check-unavailable")) {
        if ($marker -eq "no-swap-check-unavailable") { continue }
        if (!$ascii.Contains($marker)) { throw "Missing V206 exact diagnostic: $marker" }
    }
    $cache = Get-Content 'build/CMakeCache.txt' -Raw
    if (!$cache.Contains('DSRRL_V206_SPC25_EQUIPMENT_ID:BOOL=ON')) { throw "V206 adapter not compiled" }
    if ($ascii.Contains("c5330_s")) { throw "NPC SPC25 legacy c5330_s leaked into V206 binary" }
}
if ($env:DSRRL_V214_EXACT_RESOURCE_NAME_SRV -eq "ON") {
    if (!$ascii.Contains("[DSRRL V214 RESOURCE NAME]")) {
        throw "V214 exact source-resource to SRV compiled marker missing"
    }
    $cache = Get-Content 'build/CMakeCache.txt' -Raw
    if (!$cache.Contains('DSRRL_V214_EXACT_RESOURCE_NAME_SRV:BOOL=ON')) {
        throw "V214 resource-name transport compile option disabled"
    }
}
$hash = (Get-FileHash $bin -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $bin" | Set-Content -Encoding ascii SHA256SUMS.txt
Compress-Archive $bin,SHA256SUMS.txt DSRRL_v203_CLEAN_EQUIPMENT_SPEC_EXPERIMENT.zip
Write-Host "PRODUCTION_ADDON_SHA256=$hash"
