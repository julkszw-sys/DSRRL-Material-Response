[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourcePath,

    [string]$GameDir = (Split-Path -Parent $MyInvocation.MyCommand.Path)
)

$ErrorActionPreference = "Stop"

$relativePath = "DSRRL\EnvSpec\PackedGI\PTDE_GI_ENVSPEC_PACK_RGBA.bin"
$expectedSize = 33619968
$expectedSha256 = "c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3"

function Assert-ExactEnvSpecSidecar {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "ENVSPEC_SIDECAR_${Label}_MISSING: $Path"
    }

    $item = Get-Item -LiteralPath $Path
    if ($item.Length -ne $expectedSize) {
        throw "ENVSPEC_SIDECAR_${Label}_SIZE_MISMATCH: expected=$expectedSize actual=$($item.Length) path=$Path"
    }

    $actualSha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualSha256 -ne $expectedSha256) {
        throw "ENVSPEC_SIDECAR_${Label}_HASH_MISMATCH: expected=$expectedSha256 actual=$actualSha256 path=$Path"
    }

    return $actualSha256
}

if ([string]::IsNullOrWhiteSpace($SourcePath)) {
    throw "SourcePath is empty."
}
if ([string]::IsNullOrWhiteSpace($GameDir)) {
    throw "GameDir is empty."
}

$source = (Resolve-Path -LiteralPath $SourcePath).Path
$root = (Resolve-Path -LiteralPath $GameDir).Path
$null = Assert-ExactEnvSpecSidecar -Path $source -Label "SOURCE"

$target = Join-Path $root $relativePath
$targetDir = Split-Path -Parent $target
New-Item -ItemType Directory -Force -Path $targetDir | Out-Null

$sourceFull = [System.IO.Path]::GetFullPath($source)
$targetFull = [System.IO.Path]::GetFullPath($target)

if ([string]::Equals(
        $sourceFull,
        $targetFull,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    $null = Assert-ExactEnvSpecSidecar -Path $target -Label "DESTINATION"
    Write-Host "ENVSPEC_SIDECAR_STAGE_PASS path=$target size=$expectedSize sha256=$expectedSha256 source=already_staged"
    exit 0
}

$temporary = "$target.stage"
try {
    if (Test-Path -LiteralPath $temporary) {
        Remove-Item -LiteralPath $temporary -Force
    }

    Copy-Item -LiteralPath $source -Destination $temporary -Force
    $null = Assert-ExactEnvSpecSidecar -Path $temporary -Label "STAGED_TEMP"

    Move-Item -LiteralPath $temporary -Destination $target -Force
    $null = Assert-ExactEnvSpecSidecar -Path $target -Label "DESTINATION"
} finally {
    if (Test-Path -LiteralPath $temporary) {
        Remove-Item -LiteralPath $temporary -Force
    }
}

Write-Host "ENVSPEC_SIDECAR_STAGE_PASS path=$target size=$expectedSize sha256=$expectedSha256 source=$source"
