# V258 exact V253+V254+V255 single-addon source chain + dormant DXBC V258.
# Never bind V258 PS to a DSR draw until full sidecar/receiver ABI is proven.
$ErrorActionPreference='Stop'
$envDxbc='v258_ptde_env_local_subcut.dxbc'
$termDxbc='v258_ptde_phn_terminal_subcut.dxbc'
$hdr='renderer-core/include/dsrrl/runtime/generated_v258_equipment_lerp_dxbc.hpp'
$src='renderer-core/tools/build_v253_ptde_envspec_rgbm.ps1'
$tmp='renderer-core/tools/_v258_generated_native_build.ps1'
if(Test-Path $hdr){throw 'Existing V258 generated header, refusing overwrite'}
if(Test-Path $tmp){throw 'Existing V258 temporary native builder, refusing overwrite'}
try{
    # D3DCompile and D3DReflect must run on the same Windows builder,
    # not use an assumed stable DXBC output from another environment.
    cl.exe /nologo /std:c++17 /EHsc /W4 /WX renderer-core/tools/v258/compile_and_verify_ptde_gpu_subcuts.cpp /Fe:v258-dxbc-oracle.exe /link d3dcompiler.lib
    if($LASTEXITCODE -ne 0){throw 'V258 two DXBC builder compile failure'}
    & ./v258-dxbc-oracle.exe
    if($LASTEXITCODE -ne 0){throw 'V258 DXBC reflection ABI failure'}
    python renderer-core/tools/v258/embed_v258_dx11_candidate.py --env $envDxbc --terminal $termDxbc --out $hdr
    if($LASTEXITCODE -ne 0){throw 'V258 DXBC header embedding failure'}
    $s=[IO.File]::ReadAllText($src).Replace([string][char]13+[char]10,[string][char]10)
    # Anchor by the unique semantic oracle label, not PowerShell here-string
    # whitespace (which may be normalized by the Windows runner).
    $unique='V253 PTDE EnvSpec exact source oracle fail'
    $at=$s.IndexOf($unique)
    if($at -lt 0 -or $s.IndexOf($unique,$at+1) -ge 0){
      throw 'V258 V253 oracle label missing or ambiguous'
    }
    $injection=@'
 python renderer-core/tools/apply_v254_lerp_multievent_diag.py --apply
 if($LASTEXITCODE -ne 0){throw 'V258 prior V254 overlay failure'}
 python renderer-core/tools/apply_v254_lerp_multievent_diag.py
 if($LASTEXITCODE -ne 0){throw 'V258 V254 oracle failure'}
 python renderer-core/tools/apply_v255_lerp_phn_cut_diag.py --apply
 if($LASTEXITCODE -ne 0){throw 'V258 prior V255 overlay failure'}
 python renderer-core/tools/apply_v255_lerp_phn_cut_diag.py
 if($LASTEXITCODE -ne 0){throw 'V258 V255 oracle failure'}
 python renderer-core/tools/v258/apply_v258_dormant_dx11_island.py --apply
 if($LASTEXITCODE -ne 0){throw 'V258 dormant integrated DXBC overlay failure'}
 python renderer-core/tools/v258/apply_v258_dormant_dx11_island.py
 if($LASTEXITCODE -ne 0){throw 'V258 dormant integrated DXBC source oracle failure'}
'@
    $injection=$injection.TrimEnd()
    $eol=$s.IndexOf([char]10,$at)
    if($eol -lt 0){throw 'V258 expected newline after source oracle'}
    $s=$s.Insert($eol+1,$injection+[char]10)
    $old="DSRRL_V253_EXACT_PTDE_ENVSPEC_RGBM_V252_SAFE.addon64"
    $new="DSRRL_V258_EQUIPMENT_PTDE_DXBC_DORMANT_V255_SAFE.addon64"
    if(!$s.Contains($old)){throw 'V258 addon output name anchor missing'}
    $s=$s.Replace($old,$new)
    $tag="'[DSRRL V253 PTDE ENVSPEC RGBM]',"
    if($s.Split([string[]]@($tag),[StringSplitOptions]::None).Count -ne 2){
       throw 'V258 V253 binary marker anchor count differs from 1'
    }
    $added=@'
  '[DSRRL V254 LERP EVENTS]',
  '[DSRRL V255 PHN CARRIER CUT]',
  '[DSRRL V258 PTDE GPU ISLAND DORMANT]',
'@
    $s=$s.Replace($tag,$tag+[char]10+$added.TrimEnd())
    $s=$s.Replace('V253_EXACT_PTDE_LIGHTBANK_ENVSPEC_RGBM_CONSTRUCTION_PASS',
                   'V258_DORMANT_REAL_DXBC_IN_ONE_ADDON_CONSTRUCTION_PASS')
    $s=$s.Replace('V253_SINGLE_ADDON_SHA256=','V258_SINGLE_ADDON_SHA256=')
    $s=$s.Replace('V253_RUNTIME_RGBM_ACTIVATION_AND_PIXEL=OPEN',
                   'V258_DORMANT_GPU_RESOURCE_AUTHORITY_AND_PIXEL=OPEN')
    [IO.File]::WriteAllText($tmp,$s,[Text.UTF8Encoding]::new($false))
    & $tmp
    if($LASTEXITCODE -ne 0){throw 'V258 Windows integrated addon build failed'}
    if(!(Test-Path $new)){throw 'V258 one-addon binary missing'}
    $bytes=[IO.File]::ReadAllBytes($new)
    $ascii=[Text.Encoding]::ASCII.GetString($bytes)
    foreach($marker in @('[DSRRL V258 PTDE GPU ISLAND DORMANT]',
                           '[DSRRL V255 PHN CARRIER CUT]',
                           '[DSRRL V254 LERP EVENTS]',
                           '[DSRRL V253 PTDE ENVSPEC RGBM]')){
       if(!$ascii.Contains($marker)){throw "V258 final binary missing $marker"}
    }
    Write-Host "V258_NATIVE_INTEGRATED_SINGLE_ADDON_SHA256=$((Get-FileHash $new -Algorithm SHA256).Hash.ToLowerInvariant())"
    Write-Host 'V258_NATIVE_SINGLE_ADDON_BINARIES_EMBEDDED_PASS'
    Write-Host 'V258_NEW_SHADER_DRAW_BINDING=INACTIVE'
    Write-Host 'V258_PTDE_PIXEL_BEHAVIOR=OPEN'
} finally {
    if(Test-Path $tmp){Remove-Item $tmp -Force}
    if(Test-Path $hdr){Remove-Item $hdr -Force}
}
