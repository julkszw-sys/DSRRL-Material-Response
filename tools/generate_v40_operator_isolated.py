#!/usr/bin/env python3
from __future__ import annotations
import argparse,re
from pathlib import Path

SIDECARS = r'''
struct v40_shader_sidecar { const wchar_t *name; const char *sha256; std::uint32_t size; };
inline constexpr std::array<v40_shader_sidecar,3> k_v40_shader_sidecars={{
    {L"PMetal_EnvSpec_Island_stock33_v40.dxbc","088adca8ed2d6d13e3452a68f1e447af43e23b149679c9bc77ebb8f4b0ac7e35",19872u},
    {L"PMetal_EnvSpec_Island_stock34_v40.dxbc","ce63f9722960b324fcc3bf41708b0896129f7099a6b785827e3c0c628f5fb604",19572u},
    {L"PMetal_EnvSpec_Island_stock35_v40.dxbc","a74826ac6f92cd1e38d6e9d45728e6f2901bd1f91204d14bd8c099ab5d6e585d",18076u}
}};

std::filesystem::path v40_shader_path(std::size_t i)
{
    if(i>=k_v40_shader_sidecars.size())return {};
    auto p=module_path(GetModuleHandleW(nullptr));
    if(p.empty())return {};
    return p.parent_path()/L"DSRRL"/L"EnvSpec"/L"FullBridge"/L"OperatorIsland"/k_v40_shader_sidecars[i].name;
}
'''.strip()

ENSURE = r'''
bool ensure_owned_dedicated(ID3D11Device *dev)
{
    if(dev==nullptr)return false;std::lock_guard lock(g_owned_ps_mutex);
    if(g_owned_dedicated_ps[0]&&g_owned_dedicated_ps[1]&&g_owned_dedicated_ps[2])return true;
    for(auto *&p:g_owned_dedicated_ps){if(p){p->Release();p=nullptr;}}
    for(std::size_t i=0;i<k_v40_shader_sidecars.size();++i){
        std::vector<std::uint8_t> bytes;
        const auto path=v40_shader_path(i);
        if(path.empty()||!read_verified_file(path,k_v40_shader_sidecars[i].sha256,bytes)||bytes.size()!=k_v40_shader_sidecars[i].size){
            ++g_owned_ps_create_fail;for(auto *&p:g_owned_dedicated_ps){if(p){p->Release();p=nullptr;}}return false;
        }
        if(bytes.size()<32||std::memcmp(bytes.data(),"DXBC",4)!=0){++g_owned_ps_create_fail;for(auto *&p:g_owned_dedicated_ps){if(p){p->Release();p=nullptr;}}return false;}
        std::uint32_t declared=0;std::memcpy(&declared,bytes.data()+24,sizeof(declared));
        if(declared!=bytes.size()){++g_owned_ps_create_fail;for(auto *&p:g_owned_dedicated_ps){if(p){p->Release();p=nullptr;}}return false;}
        if(FAILED(dev->CreatePixelShader(bytes.data(),bytes.size(),nullptr,&g_owned_dedicated_ps[i]))||g_owned_dedicated_ps[i]==nullptr){
            ++g_owned_ps_create_fail;for(auto *&p:g_owned_dedicated_ps){if(p){p->Release();p=nullptr;}}return false;
        }
    }
    g_ps_dedicated_created.store(3,std::memory_order_relaxed);return true;
}
'''.strip()

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--input',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    text=a.input.read_text(encoding='utf-8')
    old='inline constexpr std::array<std::uintptr_t,3> k_dedicated_blob_rvas={0x18b000u,0x190250u,0x195370u};'
    if old not in text: raise SystemExit('old dedicated RVA array missing')
    text=text.replace(old,SIDECARS,1)
    pat=r'bool ensure_owned_dedicated\(ID3D11Device \*dev\)\n\{.*?\n\}\n\n(?=void release_owned_dedicated\(\)noexcept)'
    text,n=re.subn(pat,ENSURE+'\n\n',text,count=1,flags=re.S)
    if n!=1: raise SystemExit(f'ensure replacement count={n}')
    text=text.replace('V3.8 Shader-Index Gate','V4.0 Operator-Isolated EnvSpec')
    text=text.replace('[DSRRL FULL ENVSPEC V3.8]','[DSRRL FULL ENVSPEC V4.0]')
    text=text.replace('V3.8 exact shader-index P_Metal consumer island armed','V4.0 operator-isolated P_Metal EnvSpec island armed')
    text=text.replace('Exact build131 + exact P_Metal + stock shader_index 894/913/932 -> owned DXBC72/73/74 consumer island; raw PTDE RGBA; native PS+t12/t14+s12/s14 transaction; fail-open elsewhere.',
                      'Exact build131 + exact P_Metal + stock shader_index 894/913/932 -> SHA-guarded stock-body-plus-PTDE-EnvSpec operator island; raw PTDE RGBA; native PS+t12/t14+s12/s14 transaction; no legacy material-tail changes; fail-open elsewhere.')
    required=('PMetal_EnvSpec_Island_stock33_v40.dxbc','088adca8ed2d6d13e3452a68f1e447af43e23b149679c9bc77ebb8f4b0ac7e35',
              'ce63f9722960b324fcc3bf41708b0896129f7099a6b785827e3c0c628f5fb604','a74826ac6f92cd1e38d6e9d45728e6f2901bd1f91204d14bd8c099ab5d6e585d',
              'r->id==894u','r->id==913u','r->id==932u','V4.0 Operator-Isolated EnvSpec')
    for tok in required:
        if tok not in text: raise SystemExit('missing '+tok)
    if 'base+k_dedicated_blob_rvas' in text: raise SystemExit('legacy build131 dedicated blob path still active')
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(text,encoding='utf-8')
    print('PASS V4.0 operator-isolated EnvSpec sidecar consumer')
if __name__=='__main__':main()
