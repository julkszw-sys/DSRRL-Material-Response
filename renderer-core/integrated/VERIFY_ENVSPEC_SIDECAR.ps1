[CmdletBinding()]
param(
    [string]$GameDir = (Split-Path -Parent $MyInvocation.MyCommand.Path)
)

$ErrorActionPreference = "Stop"

$relativePath = "DSRRL\EnvSpec\PackedGI\PTDE_GI_ENVSPEC_PACK_RGBA.bin"
$expectedSize = 33619968
$expectedSha256 = "c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3"

if ([string]::IsNullOrWhiteSpace($GameDir)) {
    throw "GameDir is empty."
}

$root = (Resolve-Path -LiteralPath $GameDir).Path
$path = Join-Path $root $relativePath

if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
    throw "ENVSPEC_SIDECAR_MISSING: $path"
}

$item = Get-Item -LiteralPath $path
if ($item.Length -ne $expectedSize) {
    throw "ENVSPEC_SIDECAR_SIZE_MISMATCH: expected=$expectedSize actual=$($item.Length) path=$path"
}

$actualSha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualSha256 -ne $expectedSha256) {
    throw "ENVSPEC_SIDECAR_HASH_MISMATCH: expected=$expectedSha256 actual=$actualSha256 path=$path"
}

Write-Host "ENVSPEC_SIDECAR_PASS path=$path size=$expectedSize sha256=$expectedSha256"
