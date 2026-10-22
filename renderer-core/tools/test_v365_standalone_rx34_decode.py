#!/usr/bin/env python3
"""V365 isolated exact RX34 Classic t11 RGB/A shader; verify no global patch."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
h=(r/'include/dsrrl/operators/env_spec/classic_envdiffuse_rx34_decode_v365.hpp').read_text('utf8')
pipe=(r/'src/runtime/a1_create_pipeline_bridge.cpp').read_text('utf8')
decl=(r/'include/dsrrl/runtime/a1_create_pipeline_bridge.hpp').read_text('utf8')
build=(r/'tools/build_v230_envdiffuse_cinematic_trace.ps1').read_text('utf8')
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
assert 'v365_rx34_decode_candidate_.clear();' in pipe
assert 'v365_rx34_decode_candidate_=std::move(isolated)' in pipe
assert 'isolated;' in pipe
assert 'pixel_shader->code = isolated' not in pipe
assert 'material_route=UNAUTHORIZED' in pipe
assert 'bind=NO pixels=OPEN' in pipe
assert 'classic_rx34_decode_candidate' in decl
assert '/DDSRRL_V365_CLASSIC_RX34_STANDALONE_DXBC' in build
print('V365_STANDALONE_RX34_CLASSIC_RGB_DIV_ALPHA_SOURCE_PASS')
