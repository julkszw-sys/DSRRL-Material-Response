# V230: identical V229 renderer lineage, bounded exact-live companion reuse.
# The last resource hook, receiver shader payload and Lerp remain untouched.
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
python renderer-core/tools/test_v231_resource_exception_safety.py
if ($LASTEXITCODE -ne 0) { throw 'V231 safety oracle failed' }
$protected = @(
 'renderer-core/src/runtime/texture_identity_transport.cpp',
 'renderer-core/src/runtime/flver_engine_hooks.cpp',
 'renderer-core/src/runtime/envspec_resource_runtime.cpp',
 'renderer-core/src/runtime/pmetal_env_source_runtime.cpp',
 'renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp',
 'renderer-core/integrated/integrated_addon.cpp',
 'renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp',
 'renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp',
 'renderer-core/src/operators/material_response/material_response_island.cpp'

)
git diff --exit-code 75ae33d71d6859ae21d7004b47b740f0a3cb2727 HEAD -- $protected
if ($LASTEXITCODE -ne 0) { throw 'V230 protected-source contract failed' }

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
$cmakeAnchor = 'cmake -S renderer-core/integrated -B build -A x64 @flags'
if ($generated.Split([string[]]@($cmakeAnchor),[StringSplitOptions]::None).Count -ne 2) {
 throw 'V230 post-V224 CMake anchor drift'
}
$lateOverlay = @'
$path='renderer-core/src/runtime/material_resource_draw_runtime.cpp'
$text=Get-Content -LiteralPath $path -Raw
$begin=$text.IndexOf('std::filesystem::path process_dir()')
$end=$text.IndexOf('const wchar_t *class_dir(',$begin)
if ($begin -lt 0 -or $end -le $begin -or $text.LastIndexOf('std::filesystem::path process_dir()') -ne $begin) {
 throw 'V230 immutable process-dir signature missing'
}
$new=@"
const std::filesystem::path &process_dir() noexcept
{
    static const std::filesystem::path root = []() noexcept {
        try {
            std::wstring buffer(32768, L'\0');
            const DWORD n = GetModuleFileNameW(
                nullptr, buffer.data(),
                static_cast<DWORD>(buffer.size()));
            if (n == 0u || n >= buffer.size())
                return std::filesystem::path{};
            buffer.resize(n);
            return std::filesystem::path(
                buffer).parent_path();
        } catch (...) {
            return std::filesystem::path{};
        }
    }();
    return root;
}
"@
$text=$text.Substring(0,$begin)+$new+"`n"+$text.Substring($end)
$fsPattern='(?m)^ {8}if \(!std::filesystem::is_regular_file\(path\)\)\r?\n {12}return \{\};\r?\n\r?\n'
if ([regex]::Matches($text,$fsPattern).Count -ne 1) { throw 'V230 DDS redundant-file-existence check missing' }
$text=[regex]::Replace($text,$fsPattern,'')
if ($text.Split([string[]]@('    auto root = process_dir();'),[StringSplitOptions]::None).Count -ne 2) { throw 'V230 game-root single consumer mismatch' }
$text=$text.Replace('    auto root = process_dir();','    const auto &root = process_dir();')
[IO.File]::WriteAllText((Join-Path (Get-Location) $path),$text)
Write-Host 'V230_POST_V224_STOCK_RESOURCE_HOOK_PRESERVED_GAME_ROOT_CACHE_AND_SINGLE_OPEN'
'@
$generated = $generated.Replace($cmakeAnchor, $lateOverlay + [Environment]::NewLine + $cmakeAnchor)
[IO.File]::WriteAllText((Join-Path (Get-Location) $path),$generated)

./renderer-core/tools/configure_v203_clean_equipment.ps1
if ($LASTEXITCODE -ne 0) { throw 'V227 native integrated build failed' }
python renderer-core/tools/apply_v227_lerp_pmetal_nonmetal_parity.py
if ($LASTEXITCODE -ne 0) { throw 'V227 final native source mismatch' }
python renderer-core/tools/apply_v229_gloss_material_consumer_witness.py
if ($LASTEXITCODE -ne 0) { throw 'V229 final native source mismatch' }
if (!(Select-String -Path renderer-core/src/runtime/material_resource_draw_runtime.cpp -SimpleMatch -Pattern 'const std::filesystem::path &process_dir() noexcept' -Quiet)) { throw 'V230 safe filesystem optimization missing' }
Write-Host 'V230_ONE_ADDON_SAFE_DDS_IO_WITH_PROTECTED_HOOKS'
