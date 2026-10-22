#!/usr/bin/env python3
"""V365 isolated exact RX34 Classic t11 RGB/A shader; verify no global patch."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
h=(r/'include/dsrrl/operators/env_spec/classic_envdiffuse_rx34_decode_v365.hpp').read_text('utf8')
materializer=(r/'src/operators/env_spec/pmetal_rgba_materializer.cpp').read_text('utf8')
runtime=(r/'src/runtime/pmetal_envspec_draw_runtime.cpp').read_text('utf8')
decl=(r/'include/dsrrl/operators/env_spec/pmetal_rgba_materializer.hpp').read_text('utf8')
build=(r/'tools/build_v230_envdiffuse_cinematic_trace.ps1').read_text('utf8')
integrated=(r/'integrated/integrated_addon.cpp').read_text('utf8')
cmake=(r/'integrated/CMakeLists.txt').read_text('utf8')
for key in (
    'k_stock_bytes=19556u','k_candidate_bytes=k_stock_bytes+48u',
    '0xd6,0x03,0x8d,0xe4','k_dsr_t11_sample','k_dsr_source',
    'k_dsr_merge','k_ptde_rgb_div_and_mov','0x00107936u',
    'w[temps+1u]=14u','w[site+4u]=13u',
    'k_ptde_rgb_div_and_mov.begin()','checksum_container_valid',
    'fix_checksum','count!=5u','site_hits!=1u','temp_hits!=1u',
    'std::memcmp(c.tag.data(),"SHEX",4u)','std::memset(out.data()+4u,0,16u)',
    'result::candidate_ready','not_exact_rx34','declared_temp_mismatch',
    '0x00100246u,13u'):
    assert key in h,key
for forbidden in ('0x0000000cu,0x00000003u','k_ptde_rgba_envspec_chain',
                  't12_replacement','g_pack_ready','PSSetShaderResources',
                  'PSSetSamplers','global_lightbank_gain'):
    assert forbidden not in h,forbidden
assert 'g_v365_classic_candidate=std::move(candidate)' in materializer
assert 'classic_envdiffuse_rx34_v365::materialize(' in materializer
assert 'staged_v365_classic_rx34(' in materializer
assert 'staged_v365_classic_rx34(' in decl
assert 'staged_v365_classic_rx34(' in runtime
assert 'outcome.v365_classic_rx34_standalone_candidate_ready' in runtime
assert '[DSRRL V365 CLASSIC RX34 SHADER ISLAND]' in runtime
assert 'material_authority=OPEN bind=NO pixels=OPEN' in runtime
assert 'PSSetShaderResources(' not in runtime.split('if(outcome.receiver_id==34u &&',1)[1].split('// Final Renderer Edition policy',1)[0]
assert '$<$<COMPILE_LANGUAGE:CXX>:DSRRL_V365_CLASSIC_RX34_STANDALONE_DXBC>' in cmake
assert '[DSRRL V365 CLASSIC RX34 SHADER ISLAND]' in integrated
assert 'stage=EXACT_SOURCE_CREATED' in integrated
assert 'classic_envdiffuse_rx34_v365::materialize(' in integrated
assert 'shader_bind=NO pixel=OPEN' in integrated
# Critical active-path regression: physical cut disables drawtime islands;
# this shader construction must occur in the reachable create-time path.
live_fn=integrated.split(
    'bool on_create_pipeline(\\n    reshade::api::device *device,',1)[1]
pre_early=live_fn.split('if (!k_drawtime_islands_runtime_enabled)',1)[0]
assert '#if defined(DSRRL_V365_CLASSIC_RX34_STANDALONE_DXBC)' in pre_early
assert 'classic_envdiffuse_rx34_v365::materialize(' in pre_early
assert 'stage=EXACT_SOURCE_CREATED' in pre_early
assert 'shader_bind=NO pixel=OPEN' in pre_early
v365_live=pre_early
assert 'g_core.features().enabled' not in v365_live
assert 'pixel_shader->code =' not in v365_live
assert 'CreatePixelShader(' not in v365_live
assert '/DDSRRL_V365_CLASSIC_RX34_STANDALONE_DXBC' in build
print('V365_STANDALONE_RX34_CLASSIC_RGB_DIV_ALPHA_SOURCE_PASS')
