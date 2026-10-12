# V228: reproducible opt-in overlay on the already verified V224 build.
# Source donor, ReShade pin, P_Metal and all other operator switches unchanged.
$ErrorActionPreference = 'Stop'
$required = @(
 'DSRRL_V203_EQUIPMENT_ENVSPEC36',
 'DSRRL_V203_FULL_SIDECAR_SPECULAR',
 'DSRRL_V220_NONMETAL_SOURCE_JOIN',
 'DSRRL_V221_NONMETAL_PROBE_CONTEXT_RETRY',
 'DSRRL_V222_EXACT_PROBE_REJECT_FRONTIER',
 'DSRRL_V223_EXACT_NONMETAL_SHARED_LIGHTBANK_FALLBACK',
 'DSRRL_V224_EXACT_NATIVE_T12_RESOURCE_JOIN'
)
foreach ($name in $required) {
 if ([Environment]::GetEnvironmentVariable($name) -ne 'ON') {
   throw "V228 requires full V224 build lineage: $name"
 }
}
python renderer-core/tools/test_v227_lerp_pmetal_nonmetal_parity.py
if ($LASTEXITCODE -ne 0) { throw 'V227 baseline source contract failed' }
python renderer-core/tools/test_v228_nonmetal_lerp_stock_isolation.py
if ($LASTEXITCODE -ne 0) { throw 'V228 nonmetal Lerp full-stock A/B source contract failed' }

$path = 'renderer-core/tools/configure_v203_clean_equipment.ps1'
$original = Get-Content -LiteralPath $path -Raw
$anchor = '$flags = @('
if ($original.Split([string[]]@($anchor),[StringSplitOptions]::None).Count -ne 2) {
 throw 'V228 upstream config anchor drift'
}
$overlay = @'
python renderer-core/tools/apply_v227_lerp_pmetal_nonmetal_parity.py --apply
if ($LASTEXITCODE -ne 0) { throw "V228 Lerp source overlay failed" }
python renderer-core/tools/apply_v227_lerp_pmetal_nonmetal_parity.py
if ($LASTEXITCODE -ne 0) { throw "V227 parity source postcondition failed" }
python renderer-core/tools/apply_v228_nonmetal_lerp_stock_isolation.py --apply
if ($LASTEXITCODE -ne 0) { throw "V228 full nonmetal Lerp stock isolation apply failed" }
python renderer-core/tools/apply_v228_nonmetal_lerp_stock_isolation.py
if ($LASTEXITCODE -ne 0) { throw "V228 full nonmetal Lerp isolation source drift" }
'@
$generated = $original.Replace($anchor, $overlay + [Environment]::NewLine + $anchor)
$flagOld = '/DDSRRL_V203_EXACT_EQUIPMENT_SPEC /EHsc'
$flagNew = '/DDSRRL_V203_EXACT_EQUIPMENT_SPEC /DDSRRL_V227_LERP_PM_NONMETAL_PARITY /DDSRRL_V228_NONMETAL_LERP_STOCK_ISOLATION /EHsc'
if ($generated.Split([string[]]@($flagOld),[StringSplitOptions]::None).Count -ne 2) {
 throw 'V228 C++ flag anchor drift'
}
$generated = $generated.Replace($flagOld,$flagNew)
[IO.File]::WriteAllText((Join-Path (Get-Location) $path),$generated)

./renderer-core/tools/configure_v203_clean_equipment.ps1
if ($LASTEXITCODE -ne 0) { throw 'V228 native integrated build failed' }
python renderer-core/tools/apply_v227_lerp_pmetal_nonmetal_parity.py
if ($LASTEXITCODE -ne 0) { throw 'V227 parity source mismatch' }
python renderer-core/tools/apply_v228_nonmetal_lerp_stock_isolation.py
if ($LASTEXITCODE -ne 0) { throw 'V228 full-stock isolation source mismatch' }
Write-Host 'V228_ONE_ADDON_NONMETAL_LERP_STOCK_AB_PMETAL_UNCHANGED'
