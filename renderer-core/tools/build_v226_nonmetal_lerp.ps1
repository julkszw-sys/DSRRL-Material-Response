# V226: reproducible opt-in overlay on the already verified V224 build.
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
   throw "V226 requires full V224 build lineage: $name"
 }
}
python renderer-core/tools/test_v226_nonmetal_lerp_keyed_source.py
if ($LASTEXITCODE -ne 0) { throw 'V226 source contract failed' }

$path = 'renderer-core/tools/configure_v203_clean_equipment.ps1'
$original = Get-Content -LiteralPath $path -Raw
$anchor = '$flags = @('
if ($original.Split([string[]]@($anchor),[StringSplitOptions]::None).Count -ne 2) {
 throw 'V226 upstream config anchor drift'
}
$overlay = @'
python renderer-core/tools/apply_v226_nonmetal_lerp_keyed_source.py --apply
if ($LASTEXITCODE -ne 0) { throw "V226 Lerp source overlay failed" }
python renderer-core/tools/apply_v226_nonmetal_lerp_keyed_source.py
if ($LASTEXITCODE -ne 0) { throw "V226 source postcondition failed" }
'@
$generated = $original.Replace($anchor, $overlay + [Environment]::NewLine + $anchor)
$flagOld = '/DDSRRL_V203_EXACT_EQUIPMENT_SPEC /EHsc'
$flagNew = '/DDSRRL_V203_EXACT_EQUIPMENT_SPEC /DDSRRL_V226_NONMETAL_LERP_KEYED_SOURCE /EHsc'
if ($generated.Split([string[]]@($flagOld),[StringSplitOptions]::None).Count -ne 2) {
 throw 'V226 C++ flag anchor drift'
}
$generated = $generated.Replace($flagOld,$flagNew)
[IO.File]::WriteAllText((Join-Path (Get-Location) $path),$generated)

./renderer-core/tools/configure_v203_clean_equipment.ps1
if ($LASTEXITCODE -ne 0) { throw 'V226 native integrated build failed' }
python renderer-core/tools/apply_v226_nonmetal_lerp_keyed_source.py
if ($LASTEXITCODE -ne 0) { throw 'V226 final native source mismatch' }
Write-Host 'V226_FULL_LINEAGE_ONE_ADDON_BUILT_SOURCE_KEYED_LERP'
