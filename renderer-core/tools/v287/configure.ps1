param([string]$BuildDirectory='build-v287')
$existing=Get-Content '.github/workflows/pmetal-r19-lerp-exact-envdiffuse-fast.yml'
$flags=@($existing | Where-Object {$_ -match '^-DDSRRL_'})
$flags=@($existing | Where-Object {$_ -match '^\s+-DDSRRL_[A-Z0-9_]+=ON\s*$'} | ForEach-Object {$_.Trim()})
if($flags.Count -lt 14){throw 'R43 configuration incomplete'}
$flags += '-DDSRRL_PMETAL_V287_LERP_CERTIFIED_ONLY=ON'
$args=@('-S','renderer-core/integrated','-B',$BuildDirectory,'-A','x64')
$args+=("-DRESHADE_INCLUDE_DIR=$PWD/_reshade/include")
$args+=('-DDSRRL_BUILD_FLAVOR=pmetal_v287_r43_descendant')
& cmake @args @flags
if($LASTEXITCODE -ne 0){throw 'configuration failed'}
