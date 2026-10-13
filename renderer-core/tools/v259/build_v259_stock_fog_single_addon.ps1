# V259 single addon: inherited V258 shaders remain dormant.
# Add create-time stock Fog/LightScattering opcode preservation gate,
# scoped only to exact RX33/34/35 donor SHA. No Fog shader/CB modification.
$ErrorActionPreference='Stop'
$root='renderer-core/tools/v258/build_v258_ptde_equipment_dormant_one_addon.ps1'
$tmp='renderer-core/tools/_v259_generated_native_build.ps1'
$old='DSRRL_V258_EQUIPMENT_PTDE_DXBC_DORMANT_V255_SAFE.addon64'
$new='DSRRL_V259_STOCK_DSR_FOG_GUARD_V258_SAFE.addon64'
if(Test-Path $tmp){throw 'V259 temporary builder already exists'}
try {
    $s=[IO.File]::ReadAllText($root).Replace([string][char]13+[char]10,[string][char]10)
    $needle=" if("+'$LASTEXITCODE'+" -ne 0){throw 'V258 dormant integrated DXBC source oracle failure'}"
    # Distinguish source generator order: V259 is inserted after V258.
    if($s.Split([string[]]@($needle),[StringSplitOptions]::None).Count -ne 2){
       throw 'V259 V258 overlay oracle insertion point missing/ambiguous'
    }
    $injection=@'
 python renderer-core/tools/v259/apply_v259_stock_dsr_fog_ls_guard.py --apply
 if($LASTEXITCODE -ne 0){throw 'V259 stock DSR Fog LS guard overlay failure'}
 python renderer-core/tools/v259/apply_v259_stock_dsr_fog_ls_guard.py
 if($LASTEXITCODE -ne 0){throw 'V259 Fog LS overlay idempotence failure'}
'@
    $s=$s.Replace($needle,$needle+[char]10+$injection.TrimEnd())
    if(!$s.Contains($old)){throw 'V259 original addon filename not found'}
    $s=$s.Replace($old,$new)
    $s=$s.Replace('V258_DORMANT_REAL_DXBC_IN_ONE_ADDON_CONSTRUCTION_PASS',
                  'V259_STOCK_DSR_FOG_GUARD_CONSTRUCTION_PASS')
    $s=$s.Replace('V258_SINGLE_ADDON_SHA256=','V259_SINGLE_ADDON_SHA256=')
    [IO.File]::WriteAllText($tmp,$s,[Text.UTF8Encoding]::new($false))
    & $tmp
    if($LASTEXITCODE -ne 0){throw 'V259 Windows single addon compile failure'}
    if(!(Test-Path $new)){throw 'V259 final single addon missing'}
    $binary=[IO.File]::ReadAllBytes($new)
    $ascii=[Text.Encoding]::ASCII.GetString($binary)
    foreach($safety in @(
      '[DSRRL V258 PTDE GPU ISLAND DORMANT]',
      '[DSRRL V255 PHN CARRIER CUT]',
      '[DSRRL V254 LERP EVENTS]',
      '[DSRRL V253 PTDE ENVSPEC RGBM]',
      'bf52da4e8b27d14b56cd047e67d8b012b2824309c7b55ca64e9ba536940286b0')){
        if(!$ascii.Contains($safety)){
            throw "V259 final binary missing $safety"}
    }
    Write-Host 'V259_EXACT_STOCK_DSR_FOG_LS_GUARD_NATIVE_BUILD_PASS'
    Write-Host "V259_SINGLE_ADDON_SHA256=$( (Get-FileHash $new -Algorithm SHA256).Hash.ToLowerInvariant() )"
    Write-Host 'V259_FOG_MODIFICATION=NONE'
    Write-Host 'V259_LIGHTSCATTERING_MODIFICATION=NONE'
    Write-Host 'V259_NEW_PTDE_SHADER_BIND=OFF'
    Write-Host 'V259_OWNER_RUNTIME_AND_PTDE_PIXEL=OPEN'
} finally {
    if(Test-Path $tmp){Remove-Item $tmp -Force}
}
