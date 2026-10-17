param(
    [string]$GameDir = "",
    [string]$AddonPath = "",
    [switch]$Apply,
    [string]$Restore = ""
)

# V350 RC1: conservative exact-specular low-LOD cleanup.
# Default = audit only. -Apply MOVES files to a reversible quarantine.
# Refuse write operations unless the installed native addon matches the
# exact released V350 GPU Resident RC1 binary SHA-256.
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$expectedCorpus = "6f417110f6085ca8389213578eb9c0a86c100070717a9df20d62790052c151e4"
$expectedAddon = "c298ca9e4f4c34a427ff29d7a72fba02fab8f0f9019de2fd1098a1c73d9ba5fa"
$corpusPath = Join-Path $PSScriptRoot "DSRRL_SPEC_NAMES_1295.txt"
$directLowLod = @(
    "am_f_9450_l_s", "am_m_9372_l_s", "bd_m_9440_1_l_s",
    "bd_m_9440_2_l_s", "wp_a_0652_l_s"
)
$unresolved = @("wp_a_0911_s", "wp_a_1004_s")

function New-ExactSet {
    return ,([System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase))
}
function Die([string]$message) {
    throw "ABORT - bez zmian w sidecarze: $message"
}
function Sha256([string]$path) {
    # Pure .NET hash avoids a dependency on Get-FileHash autoloading in
    # Windows PowerShell launched through a .bat file.
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $stream = $null
    try {
        $stream = [System.IO.File]::OpenRead($path)
        $bytes = $sha.ComputeHash($stream)
        return [System.BitConverter]::ToString($bytes).Replace("-", "").ToLowerInvariant()
    } finally {
        if ($null -ne $stream) { $stream.Dispose() }
        $sha.Dispose()
    }
}
function UnderRoot([string]$path, [string]$dir) {
    $root = [IO.Path]::GetFullPath($dir).TrimEnd([char]'\') + '\'
    $resolved = [IO.Path]::GetFullPath($path)
    return $resolved.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)
}

if (-not (Test-Path -LiteralPath $corpusPath -PathType Leaf)) {
    Die "Brak manifestu referencyjnego przy skrypcie: $corpusPath"
}
if ((Sha256 $corpusPath) -ne $expectedCorpus) {
    Die "Manifest 1295 nazw ma nieprawidlowy SHA256."
}
$names = @(Get-Content -LiteralPath $corpusPath -Encoding UTF8 |
           Where-Object { $_.Length -gt 0 })
if ($names.Count -ne 1295) { Die "Manifest nie ma dokladnie 1295 nazw." }
$known = New-ExactSet
$targets = New-ExactSet
$direct = New-ExactSet
foreach ($p in $directLowLod) { [void]$direct.Add($p) }
foreach ($n in $names) {
    if ($n -cnotmatch '^(am|bd|lg|hd|wp)_[a-z0-9_]+_s$' -or
        $n -cne $n.ToLowerInvariant() -or
        -not $known.Add($n)) {
        Die "Niepoprawna lub powtorzona nazwa w zaufanym manifescie."
    }
    if ($n.EndsWith("_l_s",[StringComparison]::Ordinal) -and
        -not $direct.Contains($n)) {
        [void]$targets.Add($n)
    }
}
if ($targets.Count -ne 544 -or $direct.Count -ne 5) {
    Die "Niezgodny census: oczekiwano 544 PTDE-only low-LOD i 5 direct DSR."
}
foreach ($p in $directLowLod) {
    if (-not $known.Contains($p) -or $targets.Contains($p)) {
        Die "Naruszona ochrona pieciu poprawnych mapowan DSR!"
    }
}
foreach ($n in $unresolved) {
    if (-not $known.Contains($n) -or $targets.Contains($n)) {
        Die "Naruszona ochrona nierozstrzygnietego weapon SpecRGB."
    }
}

if ([string]::IsNullOrWhiteSpace($GameDir)) {
    if (Test-Path -LiteralPath (Join-Path $PSScriptRoot "DarkSoulsRemastered.exe")) {
        $GameDir = $PSScriptRoot
    } elseif ((Split-Path -Leaf $PSScriptRoot) -ieq "DSRRL") {
        $GameDir = Split-Path -Parent $PSScriptRoot
    } else { Die "Skopiuj zestaw obok DarkSoulsRemastered.exe lub podaj -GameDir." }
}
$GameDir = [IO.Path]::GetFullPath($GameDir)
$exe = Join-Path $GameDir "DarkSoulsRemastered.exe"
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    Die "Wskazany folder nie zawiera DarkSoulsRemastered.exe."
}
$specDir = Join-Path $GameDir "DSRRL\Specular"
# Never move files through junctions/symlinks, even if names match.
$specInfo = Get-Item -LiteralPath $specDir -ErrorAction Stop
if (($specInfo.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
    Die "Folder Specular jest junction/symlink; nie mozna bezpiecznie usuwac."
}

if (-not (Test-Path -LiteralPath $specDir -PathType Container)) {
    Die "Nie znaleziono folderu DSRRL\Specular."
}
$quarantineRoot = Join-Path $GameDir "_DSRRL_LOWLOD_QUARANTINE"
$auditRoot = Join-Path $GameDir "_DSRRL_LOWLOD_REPORTS"

# Only the exact tested RC1 has the certified negative mapping of 544 names.
# If a future addon changes receiver/resource routes, fail closed.
if ($Apply) {
    $addonCandidates = @()
    if (-not [string]::IsNullOrWhiteSpace($AddonPath)) {
        if (-not (Test-Path -LiteralPath $AddonPath -PathType Leaf)) {
            Die "Nie znaleziono -AddonPath."
        }
        $addonCandidates = @((Get-Item -LiteralPath $AddonPath))
    } else {
        foreach ($folder in @($GameDir,
                              (Join-Path $GameDir "reshade-addons"),
                              (Join-Path $GameDir "ReShade-Addons"))) {
            if (Test-Path -LiteralPath $folder -PathType Container) {
                $addonCandidates += @(Get-ChildItem -LiteralPath $folder -File -Filter "*.addon64")
            }
        }
    }
    $addonVerified = $false
    foreach ($addon in $addonCandidates) {
        if ((Sha256 $addon.FullName) -eq $expectedAddon) {
            $addonVerified = $true; break
        }
    }
    if (-not $addonVerified) {
        Die "Nie odnaleziono DSRRL V350 GPU Resident RC1 o oczekiwanym SHA256. Podaj -AddonPath lub uzyj wersji RC1. Dla innego buildu mapowanie moze byc inne."
    }
}

if (-not [string]::IsNullOrWhiteSpace($Restore)) {
    $batch = [IO.Path]::GetFullPath($Restore)
    if (-not (UnderRoot $batch $quarantineRoot)) {
        Die "Przywracanie dozwolone wylacznie z lokalnego _DSRRL_LOWLOD_QUARANTINE."
    }
    $ledger = Join-Path $batch "ledger.csv"
    if (-not (Test-Path -LiteralPath $ledger -PathType Leaf)) {
        Die "Brak ledger.csv w wybranej kwarantannie."
    }
    $records = @(Import-Csv -LiteralPath $ledger)
    $restored = 0
    foreach ($rec in $records) {
        $name = [string]$rec.Name
        $stem = [IO.Path]::GetFileNameWithoutExtension($name)
        if ([IO.Path]::GetFileName($name) -cne $name -or
            [IO.Path]::GetExtension($name) -ine ".dds" -or
            -not $targets.Contains($stem)) {
            Die "Ledger zawiera plik spoza dokladnych 544 kandydatow."
        }
        $from = Join-Path (Join-Path $batch "Specular") $name
        $to = Join-Path $specDir $name
        if (-not (Test-Path -LiteralPath $from -PathType Leaf)) { continue }
        if (Test-Path -LiteralPath $to) {
            Write-Warning "Pominieto $name - plik w Specular juz istnieje."
            continue
        }
        if ((Sha256 $from) -ne ([string]$rec.SHA256).ToLowerInvariant()) {
            Die "Kwarantanna nie przeszla SHA256: $name"
        }
        Move-Item -LiteralPath $from -Destination $to -ErrorAction Stop
        $restored++
    }
    Write-Host "Przywrocono plikow: $restored" -ForegroundColor Green
    exit 0
}

# Protect any potential remap mentioned in local sidecar mapping manifests.
# False positives are acceptable: they only prevent cleanup.
$protected = New-ExactSet
foreach ($p in $directLowLod) { [void]$protected.Add($p) }
$manifestSources = New-Object 'System.Collections.Generic.List[string]'
$manifestDir = Join-Path $GameDir "DSRRL\manifests"
if (Test-Path -LiteralPath $manifestDir -PathType Container) {
    foreach ($f in @(Get-ChildItem -LiteralPath $manifestDir -File -Recurse -ErrorAction Stop)) {
        $manifestSources.Add($f.FullName)
    }
}
foreach ($f in @(Get-ChildItem -LiteralPath (Join-Path $GameDir "DSRRL") -File -ErrorAction Stop)) {
    if ($f.Extension -in @(".json",".jsonl",".csv",".tsv",".yml",".yaml",".toml",".ini",".txt")) {
        $manifestSources.Add($f.FullName)
    }
}
$extraProtect = Join-Path $GameDir "DSRRL_LOWLOD_PROTECT.txt"
if (Test-Path -LiteralPath $extraProtect -PathType Leaf) {
    $manifestSources.Add($extraProtect)
}
foreach ($file in $manifestSources) {
    $fi = Get-Item -LiteralPath $file -ErrorAction Stop
    if (($fi.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        Die "Manifest jest linkiem - nie mozna bezpiecznie zweryfikowac: $file"
    }
    if ($fi.Length -gt 33554432) {
        Die "Manifest wiekszy niz 32 MB - wymaga recznego sprawdzenia: $file"
    }
    $body = [IO.File]::ReadAllText($file)
    $mentions = [regex]::Matches($body,
        '(?i)(?<![a-z0-9_])(?:am|bd|lg|hd|wp)_[a-z0-9_]+_l_s(?:\.dds)?(?![a-z0-9_])')
    foreach ($m in $mentions) {
        $name = $m.Value.ToLowerInvariant()
        if ($name.EndsWith(".dds")) { $name = $name.Substring(0,$name.Length-4) }
        if ($targets.Contains($name)) { [void]$protected.Add($name) }
    }
}

$all = @(Get-ChildItem -LiteralPath $specDir -File -ErrorAction Stop |
         Sort-Object -Property Name)
$rows = New-Object 'System.Collections.Generic.List[object]'
$toMove = New-Object 'System.Collections.Generic.List[object]'
foreach ($f in $all) {
    $stem = [IO.Path]::GetFileNameWithoutExtension($f.Name)
    $status = "KEEP_NOT_EXACT_NEGATIVE_MAPPING"
    if ($f.Extension -ieq ".dds" -and $targets.Contains($stem)) {
        if (($f.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            $status = "KEEP_REPARSE_POINT"
        } elseif ($protected.Contains($stem)) {
            $status = "KEEP_LOCAL_MAPPING_REFERENCE"
        } else {
            $status = "REMOVE_FROM_ACTIVE_QUARANTINE"
            $toMove.Add($f)
        }
    } elseif ($f.Extension -ieq ".dds" -and $direct.Contains($stem)) {
        $status = "KEEP_DSR_DIRECT_LOD"
    }
    $rows.Add([pscustomobject]@{
        Name = $f.Name
        Bytes = $f.Length
        Decision = $status
    })
}
New-Item -ItemType Directory -Path $auditRoot -Force | Out-Null
$stamp = Get-Date -Format "yyyyMMdd_HHmmss_fff"
$report = Join-Path $auditRoot ("low_lod_audit_" + $stamp + ".csv")
$rows | Export-Csv -NoTypeInformation -LiteralPath $report -Encoding UTF8
Write-Host "DSRRL V350 RC1 LOW-LOD - audyt dokladnych 544 negative mappings"
Write-Host "Chronione DSR-direct low LOD: 5 | niewyjasnione: 2 (nietkniete)"
Write-Host "W katalogu Specular: $($all.Count) | do kwarantanny: $($toMove.Count)"
Write-Host "Wpisy kandydatow chronione lokalnymi manifestami: $($protected.Count - 5)"
Write-Host "Raport: $report"

if (-not $Apply) {
    Write-Host "DRY RUN - NIC NIE ZMIENIONO. Uruchom z -Apply aby przeniesc kandydatow." -ForegroundColor Yellow
    exit 0
}
if ($toMove.Count -eq 0) {
    Write-Host "Brak plikow do usuniecia z aktywnego sidecaru."
    exit 0
}
$batch = Join-Path $quarantineRoot $stamp
$dest = Join-Path $batch "Specular"
if (Test-Path -LiteralPath $batch) { Die "Katalog kwarantanny juz istnieje." }
New-Item -ItemType Directory -Path $dest -Force | Out-Null
$ledgerRows = New-Object 'System.Collections.Generic.List[object]'
foreach ($f in $toMove) {
    $ledgerRows.Add([pscustomobject]@{
        Name = $f.Name
        SHA256 = Sha256 $f.FullName
        Bytes = $f.Length
    })
}
$ledger = Join-Path $batch "ledger.csv"
$ledgerRows | Export-Csv -NoTypeInformation -LiteralPath $ledger -Encoding UTF8
$moved = 0
foreach ($r in $ledgerRows) {
    $from = Join-Path $specDir $r.Name
    $to = Join-Path $dest $r.Name
    if (Test-Path -LiteralPath $to) { Die "Kolizja w kwarantannie: $($r.Name)" }
    if ((Sha256 $from) -ne $r.SHA256) { Die "Plik zmienil sie od audytu: $($r.Name)" }
    Move-Item -LiteralPath $from -Destination $to -ErrorAction Stop
    if ((Sha256 $to) -ne $r.SHA256) {
        Move-Item -LiteralPath $to -Destination $from -ErrorAction SilentlyContinue
        Die "Niezgodnosc SHA256 po przeniesieniu: $($r.Name)"
    }
    $moved++
}
Write-Host "USUNIETO Z AKTYWNEGO SIDECARU: $moved plikow." -ForegroundColor Green
Write-Host "Mozna przywrocic: .\DSRRL_SAFE_CLEAN_LOWLOD.ps1 -Restore `"$batch`""
