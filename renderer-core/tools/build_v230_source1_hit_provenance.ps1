param([Parameter(Mandatory=$true)][string]$ReShadeInclude)
$ErrorActionPreference='Stop'
$baseScript=Join-Path $PSScriptRoot 'build_v230_production.ps1'
$text=[IO.File]::ReadAllText($baseScript)
function Replace-Once([string]$from,[string]$to) {
  $at=$script:text.IndexOf($from)
  if ($at -lt 0 -or $script:text.IndexOf($from,$at+$from.Length) -ge 0) {
    throw "Source1 hit diagnostic wrapper anchor mismatch"
  }
  $script:text=$script:text.Substring(0,$at)+$to+$script:text.Substring($at+$from.Length)
}
Replace-Once '/DDSRRL_V230_SOURCE1_REPAIR ' '/DDSRRL_V230_SOURCE1_REPAIR /DDSRRL_V230_SOURCE1_EXACT_PROVENANCE '
$old="if ($LASTEXITCODE -ne 0) { throw 'v2.3.0 exact source1 repair overlay failed' }"
$new=$old+"`npython renderer-core/tools/materialize_v230_source1_exact_hit_provenance.py`nif ($LASTEXITCODE -ne 0) { throw 'v230 source1 hit provenance failed' }"
Replace-Once $old $new
$temp=Join-Path $PSScriptRoot '_v230_source1_hit_provenance_generated.ps1'
try {
  [IO.File]::WriteAllText($temp,$text,[Text.UTF8Encoding]::new($false))
  & $temp -ReShadeInclude $ReShadeInclude
  if ($LASTEXITCODE -ne 0) { throw 'Source1 hit provenance native build failed' }
} finally {
  Remove-Item $temp -Force -ErrorAction SilentlyContinue
}
