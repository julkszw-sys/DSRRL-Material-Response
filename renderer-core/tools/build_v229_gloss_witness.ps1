# V229: V227 exact material/lerp donor, add read-only stable/Lerp gloss witness.
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
   throw "V227 requires full V224 build lineage: $name"
 }
}
python renderer-core/tools/test_v227_lerp_pmetal_nonmetal_parity.py
if ($LASTEXITCODE -ne 0) { throw 'V227 source contract failed' }
python renderer-core/tools/test_v229_gloss_material_consumer_witness.py
if ($LASTEXITCODE -ne 0) { throw 'V229 material/consumer contract failed' }

$path = 'renderer-core/tools/configure_v203_clean_equipment.ps1'
$original = Get-Content -LiteralPath $path -Raw
$anchor = '$flags = @('
if ($original.Split([string[]]@($anchor),[StringSplitOptions]::None).Count -ne 2) {
 throw 'V227 upstream config anchor drift'
}
$overlay = @'
python renderer-core/tools/apply_v227_lerp_pmetal_nonmetal_parity.py --apply
if ($LASTEXITCODE -ne 0) { throw "V227 Lerp source overlay failed" }
python renderer-core/tools/apply_v227_lerp_pmetal_nonmetal_parity.py
if ($LASTEXITCODE -ne 0) { throw "V227 source postcondition failed" }
python renderer-core/tools/apply_v229_gloss_material_consumer_witness.py --apply
if ($LASTEXITCODE -ne 0) { throw "V229 read-only prepared gloss witness failed" }
python renderer-core/tools/apply_v229_gloss_material_consumer_witness.py
if ($LASTEXITCODE -ne 0) { throw "V229 generated source drift" }
'@
$generated = $original.Replace($anchor, $overlay + [Environment]::NewLine + $anchor)
$flagOld = '/DDSRRL_V203_EXACT_EQUIPMENT_SPEC /EHsc'
$flagNew = '/DDSRRL_V203_EXACT_EQUIPMENT_SPEC /DDSRRL_V227_LERP_PM_NONMETAL_PARITY /DDSRRL_V229_GLOSS_MATERIAL_CONSUMER_WITNESS /EHsc'
if ($generated.Split([string[]]@($flagOld),[StringSplitOptions]::None).Count -ne 2) {
 throw 'V227 C++ flag anchor drift'
}
$generated = $generated.Replace($flagOld,$flagNew)
[IO.File]::WriteAllText((Join-Path (Get-Location) $path),$generated)

./renderer-core/tools/configure_v203_clean_equipment.ps1
if ($LASTEXITCODE -ne 0) { throw 'V227 native integrated build failed' }
python renderer-core/tools/apply_v227_lerp_pmetal_nonmetal_parity.py
if ($LASTEXITCODE -ne 0) { throw 'V227 final native source mismatch' }
python renderer-core/tools/apply_v229_gloss_material_consumer_witness.py
if ($LASTEXITCODE -ne 0) { throw 'V229 final native source mismatch' }
Write-Host 'V229_ONE_ADDON_READONLY_STABLE_LERP_MATERIAL_GLOSS_CUT'
