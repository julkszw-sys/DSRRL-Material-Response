param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Set-Location $root
# V297 constructor is the pinned source-of-truth for cumulative islands.
& ./renderer-core/tools/build_v297_one_atomic_addon.ps1 -ReShadeInclude $ReShadeInclude
if ($LASTEXITCODE -ne 0) { throw 'V297 prerequisite failed' }
python renderer-core/tools/materialize_v298_live_mtd_witness.py
if ($LASTEXITCODE -ne 0) { throw 'V298 parser witness patch failed' }
python renderer-core/tools/test_v297_authored_slot0_source.py
if ($LASTEXITCODE -ne 0) { throw 'Exact material donor/owner source contract failed' }
$flags='/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID /DDSRRL_V297_AUTHORED_SLOT0_DIAG /DDSRRL_V298_LIVE_MTD_WITNESS'
cmake -S renderer-core/integrated -B build-v297 "-DCMAKE_CXX_FLAGS=$flags" '-DDSRRL_BUILD_FLAVOR=v298_exact_parser_witness'
if ($LASTEXITCODE -ne 0) { throw 'V298 CMake configure failed' }
cmake --build build-v297 --config Release --target dsrrl_renderer_core_integrated
if ($LASTEXITCODE -ne 0) { throw 'V298 Win64 native build failed' }
$files=@(Get-ChildItem build-v297 -Recurse -Filter *.addon64)
if ($files.Count -ne 1) { throw 'Expected exactly one addon' }
New-Item -ItemType Directory -Force out-v298 | Out-Null
$dest='out-v298/DSRRL_V298_PTDE_SLOT0_LIVE_MTD.addon64'
Copy-Item $files[0].FullName $dest -Force
$bytes=[System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes((Resolve-Path $dest)))
if (-not $bytes.Contains('v298_exact_parser_witness')) { throw 'Source flavor not attested' }
(Get-FileHash $dest -Algorithm SHA256) | Format-List
Write-Host 'V298 NATIVE BUILD PASS; RUNTIME/PIXEL OPEN'
