param([string]$ProbeScript)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if([string]::IsNullOrWhiteSpace($ProbeScript) -or !(Test-Path -LiteralPath $ProbeScript)) { throw 'Probe file absent' }
$version = & python --version
if($LASTEXITCODE -ne 0 -or "$version" -notmatch '^Python 3\.[0-9]+(\.[0-9]+)?$') {
  throw "Expected working installed Python 3, received: $version"
}
Write-Host "Using existing $version (python command)"
function Invoke-Check([string]$prog,[string[]]$Params) {
  & $prog @Params
  if($LASTEXITCODE -ne 0) { throw "FAIL OPEN child failed: $prog (exit $LASTEXITCODE)" }
}
function Invoke-Py([string[]]$Params) {
  Invoke-Check -prog 'python' -Params $Params
}
Invoke-Py -Params @($ProbeScript)
Write-Host 'V308_WINDOWS_POWERSHELL_51_PYTHON_314_ARGV_PASS'
