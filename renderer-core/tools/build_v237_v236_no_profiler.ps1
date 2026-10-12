# V237: source-identical V236 synchronization without R43 QPC instrumentation.
# One-off A/B: absolutely no renderer, shader, resource or material edits.
# Transiently neutralize V233's profiling opt-in, then run V236's exact
# source-restoring native integration. Always restore the original builder.
$ErrorActionPreference = 'Stop'
$script = 'renderer-core/tools/build_v233_transition_stutter_diagnostic.ps1'
$full = Join-Path (Get-Location) $script
$before = [IO.File]::ReadAllText($full)
$patches = @(
    @('-DDSRRL_STUTTER_PROFILE=ON','-DDSRRL_STUTTER_PROFILE=OFF'),
    @('-DDSRRL_STUTTER_HITCH_TRACE=ON','-DDSRRL_STUTTER_HITCH_TRACE=OFF'),
    @('DSRRL_STUTTER_PROFILE:BOOL=ON','DSRRL_STUTTER_PROFILE:BOOL=OFF'),
    @('DSRRL_STUTTER_HITCH_TRACE:BOOL=ON','DSRRL_STUTTER_HITCH_TRACE:BOOL=OFF')
)
$changed = $before
foreach ($pair in $patches) {
    $from = $pair[0]
    $to = $pair[1]
    if ($changed.Split([string[]]@($from),[StringSplitOptions]::None).Count -ne 2) {
        throw "V237 V233 profiler anchor drift: $from"
    }
    $changed = $changed.Replace($from,$to)
}
try {
    [IO.File]::WriteAllText($full,$changed)
    ./renderer-core/tools/build_v236_slot_sync.ps1
    if ($LASTEXITCODE -ne 0) { throw 'V237 inherited V236 native build failed' }
    $cache = 'build/CMakeCache.txt'
    foreach ($option in @('DSRRL_STUTTER_PROFILE','DSRRL_STUTTER_HITCH_TRACE')) {
        if (-not (Select-String -Path $cache -SimpleMatch -Pattern ($option + ':BOOL=OFF') -Quiet)) {
            throw "V237 option not disabled: $option"
        }
    }
    $parentAddon = 'DSRRL_V236_SLOT_SYNC_DIAG.addon64'
    if (-not (Test-Path $parentAddon)) { throw 'V237 inherited V236 addon missing' }
    $binary = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($parentAddon))
    if ($binary.Contains('[DSRRL STUTTER R43]') -or
        $binary.Contains('[DSRRL HITCH FRAME]')) {
        throw 'V237 QPC profiler diagnostic still present in binary'
    }
    $out = 'DSRRL_V237_V236_SYNCHRONIZATION_NO_PROFILER.addon64'
    Copy-Item -LiteralPath $parentAddon -Destination $out -Force
    Write-Host 'V237_V236_EXACT_SYNC_WITH_R43_QPC_PROFILER_DISABLED_PASS'
    Write-Host 'V237_ONLY_DIFFERENCE_STUTTER_QPC_AND_HITCH_COMPILE_FLAGS_OFF'
} finally {
    [IO.File]::WriteAllText($full,$before)
    if ([IO.File]::ReadAllText($full) -cne $before) {
        throw 'V237 failed to restore original V233 builder'
    }
}
