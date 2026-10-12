# V233 diagnostic-only wrapper.
# Reuses the exact V231/V232 V230 build lineage and protected render source.
# Enables the EXISTING bounded R43 Present/stage/hitch instrumentation only.
# Restore source-controlled recipe even on failure.
$ErrorActionPreference = 'Stop'
$recipe = 'renderer-core/tools/configure_v203_clean_equipment.ps1'
$original = [IO.File]::ReadAllText((Join-Path (Get-Location) $recipe))
$flags = @(
    @('" -DDSRRL_STUTTER_PROFILE=OFF"', '" -DDSRRL_STUTTER_PROFILE=ON"'),
    @('" -DDSRRL_STUTTER_HITCH_TRACE=OFF"', '" -DDSRRL_STUTTER_HITCH_TRACE=ON"')
)
$patched = $original
foreach ($pair in $flags) {
    $from = $pair[0].Replace('" ', '"')
    $to = $pair[1].Replace('" ', '"')
    if ($patched.Split([string[]]@($from), [StringSplitOptions]::None).Count -ne 2) {
        throw "V233 profiler option anchor drift: $from"
    }
    $patched = $patched.Replace($from, $to)
}
if (-not $patched.Contains('$flags = @(')) {
    throw 'V233 original V230 builder insertion point lost'
}
try {
    [IO.File]::WriteAllText((Join-Path (Get-Location) $recipe), $patched)
    ./renderer-core/tools/build_v230_source_audit_live_sidecar.ps1
    if ($LASTEXITCODE -ne 0) { throw 'V233 inherited addon construction failed' }
    $cache = 'build/CMakeCache.txt'
    if (!(Test-Path $cache)) { throw 'V233 CMake cache missing' }
    if (!(Select-String -Path $cache -SimpleMatch -Pattern 'DSRRL_STUTTER_PROFILE:BOOL=ON' -Quiet)) {
        throw 'V233 Present profile compile opt-in missing'
    }
    if (!(Select-String -Path $cache -SimpleMatch -Pattern 'DSRRL_STUTTER_HITCH_TRACE:BOOL=ON' -Quiet)) {
        throw 'V233 bounded per-frame hitch trace compile opt-in missing'
    }
    $native = @(Get-ChildItem build -Recurse -Filter *.addon64)
    if ($native.Count -ne 1) {
        throw "V233 expected exactly one native addon before verification; found $($native.Count)"
    }
    Write-Host 'V233_PRESENT_PROFILE_AND_HITCH_TRACE_INSTRUMENTATION_ONLY'
    Write-Host 'V233_STAGES_PRESENT_FLVER_SHA_DDS_MTD_REGISTRY_CREATE_TEXTURE2D'
} finally {
    [IO.File]::WriteAllText((Join-Path (Get-Location) $recipe), $original)
}
