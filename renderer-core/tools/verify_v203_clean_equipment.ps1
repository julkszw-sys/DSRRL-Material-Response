$ErrorActionPreference = "Stop"
$sha = (git rev-parse HEAD).Trim()
$native=@("dsrrl_pmetal_selector_policy_tests","dsrrl_pmetal_producer_state_tests","dsrrl_renderer_flver_identity_tests","dsrrl_renderer_resource_view_epoch_tests","dsrrl_renderer_companion_tls_way_tests")
cmake --build build/renderer-core-lib --config Release --target $native
if ($LASTEXITCODE -ne 0) { throw "Native test build failed" }
ctest --test-dir build/renderer-core-lib -C Release -R "^dsrrl_(pmetal_(selector_policy|producer_state)|renderer_(flver_identity|resource_view_epoch|companion_tls_way))_tests$" --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Native tests failed" }
if ($env:DSRRL_V203_FULL_SIDECAR_SPECULAR -eq "ON") {
    python renderer-core/tools/apply_v203_full_sidecar_1295.py
    if ($LASTEXITCODE -ne 0) { throw "Full 1295 sidecar exact-source overlay postcondition failed" }
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
    foreach ($required in @("[DSRRL SIDECAR 1295]","nonmetal_resource_probe","DSRRL_V203_FULL_SIDECAR_1295")) {
        if (!$ascii.Contains($required)) { throw "Missing full sidecar 1295 binary diagnostic $required" }
    }
}
$hash = (Get-FileHash $bin -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $bin" | Set-Content -Encoding ascii SHA256SUMS.txt
Compress-Archive $bin,SHA256SUMS.txt DSRRL_v203_CLEAN_EQUIPMENT_SPEC_EXPERIMENT.zip
Write-Host "PRODUCTION_ADDON_SHA256=$hash"
