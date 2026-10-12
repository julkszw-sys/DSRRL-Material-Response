# V244: the actual bound HemEnvLerp draw (rx 33/34) only.
# Baseline V237 = V236 source-sync, R43 QPC/hitch disabled.
# V243 is an exact temporary overlay, then V244 replaces only its observer.
$ErrorActionPreference = 'Stop'
$src = 'renderer-core/integrated/integrated_addon.cpp'

python renderer-core/tools/test_v244_true_lerp_draw_proof.py
if ($LASTEXITCODE -ne 0) { throw 'V244 source oracle failed' }
./renderer-core/tools/build_v237_v236_no_profiler.ps1
if ($LASTEXITCODE -ne 0) { throw 'V244 original no-QPC V237 baseline failed' }

$before = [IO.File]::ReadAllText($src)
try {
    python renderer-core/tools/apply_v243_rx34_draw_proof.py --apply
    if ($LASTEXITCODE -ne 0) { throw 'V244 V243 diagnostic preimage failed' }
    python renderer-core/tools/apply_v244_true_lerp_draw_proof.py --apply
    if ($LASTEXITCODE -ne 0) { throw 'V244 exact-family patch failed' }
    python renderer-core/tools/apply_v244_true_lerp_draw_proof.py
    if ($LASTEXITCODE -ne 0) { throw 'V244 observer postcondition failed' }

    # Rebuild physical-cut generated translation unit WITH temporary observer.
    cmake -S renderer-core/integrated -B build -A x64
    if ($LASTEXITCODE -ne 0) { throw 'V244 physical-cut generation failed' }
    $generated = @(Get-ChildItem 'build/generated' -Recurse -Filter 'integrated_addon_physical_cut.cpp')
    if ($generated.Count -ne 1) { throw 'V244 exact physical-cut TU missing' }
    $generatedText = [IO.File]::ReadAllText($generated[0].FullName)
    if (!$generatedText.Contains('[DSRRL V244 TRUE LERP DRAW PROOF]')) {
        throw 'V244 exact-family observer not compiled'
    }
    if ($generatedText.Contains('[DSRRL V243 RX DRAW PROOF]')) {
        throw 'V243 proxy observer still linked'
    }

    cmake --build build --config Release --target dsrrl_renderer_core_integrated
    if ($LASTEXITCODE -ne 0) { throw 'V244 native addon failed' }
    $native = 'build/Release/DSRRL_Restored_Lighting_v2_0_2.addon64'
    if (!(Test-Path $native)) { throw 'V244 native addon missing' }
    $binary = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($native))
    if (!$binary.Contains('[DSRRL V244 TRUE LERP DRAW PROOF]')) {
        throw 'V244 marker absent from binary'
    }
    if ($binary.Contains('[DSRRL V243 RX DRAW PROOF]') -or
        $binary.Contains('[DSRRL HITCH FRAME]') -or
        $binary.Contains('[DSRRL STUTTER R43]')) {
        throw 'V244 old observer or QPC still linked'
    }

    $out = 'DSRRL_V244_TRUE_LERP_FAMILY_PROOF_V237_NOQPC.addon64'
    Copy-Item -LiteralPath $native -Destination $out -Force
    Write-Host 'V244_SINGLE_ADDON_BOUND_HEMENVLERP_NOQPC_COMPLETE'
    Write-Host 'V244_NO_SHADER_PRODUCER_CB_RESOURCE_OR_POINTLIGHT_CHANGES'
} finally {
    [IO.File]::WriteAllText($src, $before)
    if ([IO.File]::ReadAllText($src) -cne $before) {
        throw 'V244 failed to restore pristine integrated source'
    }
}
