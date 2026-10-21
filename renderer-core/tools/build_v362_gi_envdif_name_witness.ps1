param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
$src=Join-Path $PSScriptRoot 'build_v230_envdiffuse_cinematic_trace.ps1'
$code=[IO.File]::ReadAllText($src)
$anchor="python renderer-core/tools/test_v230_gi_origin_probe_rc2.py"
if ($code.Split(@($anchor),[StringSplitOptions]::None).Count -ne 2) {
    throw 'V362 prior builder anchor drift'
}
$injection=@'
python renderer-core/tools/materialize_v362_gi_envdif_name_witness.py
python renderer-core/tools/materialize_v363_legacy_envspec_name.py
if ($LASTEXITCODE -ne 0) { throw 'V362 native GI name witness materializer failed' }
'@
$code=$code.Replace($anchor,$anchor+"`n"+$injection)
$temp=Join-Path $PSScriptRoot '_v362_build_generated.ps1'
try {
    [IO.File]::WriteAllText($temp,$code,[Text.UTF8Encoding]::new($false))
    & $temp -ReShadeInclude $ReShadeInclude
    if ($LASTEXITCODE -ne 0) { throw 'V362 native build failed' }
    $bin='out-v230-release/DSRRL_v230_PTDE_EnvDiffuse_Quelaag_Cutscene_EDC.addon64'
    if (-not (Test-Path $bin)) { throw 'Missing one addon' }
    $ascii=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes((Resolve-Path $bin)))
    if (-not $ascii.Contains('[DSRRL V363 LEGACY SPEC CREATE]') -or
        -not $ascii.Contains('[DSRRL V363 LEGACY SPEC VIEW]')) {
        throw 'V363 legacy spec name diagnostic not compiled'
    }
    if (-not $ascii.Contains('[DSRRL V362 T11 GI NAME JOIN]')) {
        throw 'V362 GI name source-to-native DX11 witness not compiled'
    }
    Get-FileHash $bin -Algorithm SHA256 | Format-List
    Write-Host 'V362 SOURCE/WIN64=PASS; NATIVE_T11_ID_OPEN; PTDE_PIXEL_OPEN'
} finally {
    Remove-Item $temp -Force -ErrorAction SilentlyContinue
}
