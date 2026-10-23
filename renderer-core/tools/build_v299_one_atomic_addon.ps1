param([string]$ReShadeInclude)
$ErrorActionPreference='Stop'
& ./renderer-core/tools/build_v298_one_atomic_addon.ps1 -ReShadeInclude $ReShadeInclude
if($LASTEXITCODE -ne 0){throw 'V298 failed'}
python renderer-core/tools/materialize_v299_slot0_source_consumer.py
if($LASTEXITCODE -ne 0){throw 'V299 producer failed'}
python renderer-core/tools/materialize_v299_oct10_candidate_census.py
if($LASTEXITCODE -ne 0){throw 'V299 census failed'}
python renderer-core/tools/test_v299_source_consumer_contract.py
if($LASTEXITCODE -ne 0){throw 'V299 source test failed'}
$flags='/DDSRRL_V296_ATOMIC_SIDECAR_NO_HYBRID /DDSRRL_V297_AUTHORED_SLOT0_DIAG /DDSRRL_V298_LIVE_MTD_WITNESS /DDSRRL_V299_AUTHORED_SLOT0_OPERATOR'
cmake -S renderer-core/integrated -B build-v297 "-DCMAKE_CXX_FLAGS=$flags" '-DDSRRL_BUILD_FLAVOR=v299_slot0_atomic'
if($LASTEXITCODE -ne 0){throw 'configure failed'}
cmake --build build-v297 --config Release --target dsrrl_renderer_core_integrated
if($LASTEXITCODE -ne 0){throw 'native build failed'}
$found=@(Get-ChildItem build-v297 -Recurse -Filter *.addon64)
if($found.Count -ne 1){throw 'expected one addon'}
New-Item -ItemType Directory -Force out-v299 | Out-Null
Copy-Item $found[0].FullName out-v299/DSRRL_V299_PTDE_SLOT0_ATOMIC_DIAG.addon64 -Force
