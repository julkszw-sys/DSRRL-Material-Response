param([string]$ReShadeInclude)
$ErrorActionPreference='Stop'
& ./renderer-core/tools/build_v299_one_atomic_addon.ps1 -ReShadeInclude $ReShadeInclude
if($LASTEXITCODE -ne 0){throw 'V299 source/build failed'}
python renderer-core/tools/materialize_v300_leather_operator.py
if($LASTEXITCODE -ne 0){throw 'V300 operator source failed'}
python renderer-core/tools/materialize_v300_retail_leather_owner.py
if($LASTEXITCODE -ne 0){throw 'V300 MTD parser failed'}
python renderer-core/tools/materialize_v300_leather_census.py
if($LASTEXITCODE -ne 0){throw 'V300 census failed'}
python renderer-core/tools/test_v300_leather_atomic_source.py
if($LASTEXITCODE -ne 0){throw 'V300 source contract failed'}
$flags='/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID /DDSRRL_V297_AUTHORED_SLOT0_DIAG /DDSRRL_V298_LIVE_MTD_WITNESS /DDSRRL_V299_AUTHORED_SLOT0_OPERATOR /DDSRRL_V300_LEATHER_SLOT1_ATOMIC'
cmake -S renderer-core/integrated -B build-v297 "-DCMAKE_CXX_FLAGS=$flags" '-DDSRRL_BUILD_FLAVOR=v300_ptde_leather_atomic'
if($LASTEXITCODE -ne 0){throw 'V300 configure failed'}
cmake --build build-v297 --config Release --target dsrrl_renderer_core_integrated
if($LASTEXITCODE -ne 0){throw 'V300 native compile failed'}
$found=@(Get-ChildItem build-v297 -Recurse -Filter *.addon64)
if($found.Count -ne 1){throw 'Expected exactly one addon'}
New-Item -ItemType Directory -Force out-v300 | Out-Null
Copy-Item $found[0].FullName out-v300/DSRRL_V300_PTDE_LEATHER_ATOMIC_DIAG.addon64 -Force
Write-Host 'V300 NATIVE BUILD PASSED, PIXEL UNVERIFIED'
