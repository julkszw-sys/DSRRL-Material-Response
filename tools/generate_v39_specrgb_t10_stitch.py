#!/usr/bin/env python3
from __future__ import annotations
import argparse
from pathlib import Path

HOST_STITCH = r'''
constexpr std::uintptr_t k_host_t10_bind_rva = 0x113B23u;
constexpr std::array<std::uint8_t,k_detour_stolen> k_host_t10_bind_prolog = {
    0x48,0x83,0xEC,0x28,0x48,0x8B,0x01,0x4C,0x8D,0x4C,0x24,0x20,0x49,0x89,0x11};
detour_state g_host_t10_detour;
using host_t10_bind_fn = void (__fastcall *)(ID3D11DeviceContext *,ID3D11ShaderResourceView *);
host_t10_bind_fn g_host_t10_original = nullptr;
thread_local ID3D11ShaderResourceView *g_host_t10_candidate = nullptr;
thread_local std::uintptr_t g_host_t10_candidate_material = 0;
std::atomic<std::uint64_t> g_t10_capture_hit{0},g_t10_capture_use{0},g_t10_capture_miss{0},g_t10_stitch_bind{0};

void clear_host_t10_candidate() noexcept
{
    if(g_host_t10_candidate!=nullptr){g_host_t10_candidate->Release();g_host_t10_candidate=nullptr;}
    g_host_t10_candidate_material=0;
}

bool active_material_is_exact_pmetal() noexcept
{
    const auto key=g_active_material;
    if(key==0)return false;
    std::lock_guard lock(g_mutex);
    const auto it=g_materials.find(key);
    return it!=g_materials.end()&&it->second.record!=nullptr&&exact_pmetal(*it->second.record);
}

void __fastcall host_t10_bind_hook(ID3D11DeviceContext *ctx,ID3D11ShaderResourceView *srv)
{
    const auto material=g_active_material;
    if(material!=0&&srv!=nullptr&&active_material_is_exact_pmetal()){
        if(g_host_t10_candidate_material!=material)clear_host_t10_candidate();
        if(g_host_t10_candidate==nullptr){srv->AddRef();g_host_t10_candidate=srv;g_host_t10_candidate_material=material;++g_t10_capture_hit;}
    }
    if(g_host_t10_original!=nullptr)g_host_t10_original(ctx,srv);
}

bool install_host_t10_stitch()
{
    HMODULE host=GetModuleHandleW(L"DSRRL_Material_Response_1.45.addon64");
    if(host==nullptr)return false;
    auto *target=reinterpret_cast<std::uint8_t *>(host)+k_host_t10_bind_rva;
    if(!install_detour(g_host_t10_detour,target,reinterpret_cast<const void *>(&host_t10_bind_hook),k_host_t10_bind_prolog))return false;
    g_host_t10_original=reinterpret_cast<host_t10_bind_fn>(g_host_t10_detour.trampoline);
    return g_host_t10_original!=nullptr;
}

void uninstall_host_t10_stitch() noexcept
{
    clear_host_t10_candidate();
    uninstall_detour(g_host_t10_detour);
    g_host_t10_original=nullptr;
}
'''.strip()


def sub_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f'{label}: anchor missing')
    return text.replace(old,new,1)


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--input',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args()
    text=a.input.read_text(encoding='utf-8')

    anchor='enum class receiver_kind:std::uint8_t{stock,dedicated}; struct receiver_rec{receiver_kind kind;std::uint32_t id;};'
    text=sub_once(text,anchor,HOST_STITCH+'\n\n'+anchor,'host stitch insertion')

    old='ID3D11DeviceContext *ctx=nullptr;ID3D11PixelShader *ps=nullptr;std::array<ID3D11ShaderResourceView *,2> srv{};std::array<ID3D11SamplerState *,2> sampler{};'
    new='ID3D11DeviceContext *ctx=nullptr;ID3D11PixelShader *ps=nullptr;ID3D11ShaderResourceView *t10=nullptr;std::array<ID3D11ShaderResourceView *,2> srv{};std::array<ID3D11SamplerState *,2> sampler{};'
    text=sub_once(text,old,new,'native state field')

    old='native_state(native_state&&o)noexcept:ctx(o.ctx),ps(o.ps),srv(o.srv),sampler(o.sampler){o.ctx=nullptr;o.ps=nullptr;o.srv={};o.sampler={};}'
    new='native_state(native_state&&o)noexcept:ctx(o.ctx),ps(o.ps),t10(o.t10),srv(o.srv),sampler(o.sampler){o.ctx=nullptr;o.ps=nullptr;o.t10=nullptr;o.srv={};o.sampler={};}'
    text=sub_once(text,old,new,'native state move ctor')

    old='native_state&operator=(native_state&&o)noexcept{if(this!=&o){release();ctx=o.ctx;ps=o.ps;srv=o.srv;sampler=o.sampler;o.ctx=nullptr;o.ps=nullptr;o.srv={};o.sampler={};}return *this;}'
    new='native_state&operator=(native_state&&o)noexcept{if(this!=&o){release();ctx=o.ctx;ps=o.ps;t10=o.t10;srv=o.srv;sampler=o.sampler;o.ctx=nullptr;o.ps=nullptr;o.t10=nullptr;o.srv={};o.sampler={};}return *this;}'
    text=sub_once(text,old,new,'native state move assign')

    old='~native_state(){release();}void release()noexcept{if(ps){ps->Release();ps=nullptr;}for(auto *&v:srv)if(v){v->Release();v=nullptr;}for(auto *&s:sampler)if(s){s->Release();s=nullptr;}ctx=nullptr;}'
    new='~native_state(){release();}void release()noexcept{if(ps){ps->Release();ps=nullptr;}if(t10){t10->Release();t10=nullptr;}for(auto *&v:srv)if(v){v->Release();v=nullptr;}for(auto *&s:sampler)if(s){s->Release();s=nullptr;}ctx=nullptr;}'
    text=sub_once(text,old,new,'native state release')

    old='s.ctx->PSGetShader(&s.ps,nullptr,nullptr);s.ctx->PSGetShaderResources(12,1,&s.srv[0]);'
    new='s.ctx->PSGetShader(&s.ps,nullptr,nullptr);s.ctx->PSGetShaderResources(10,1,&s.t10);s.ctx->PSGetShaderResources(12,1,&s.srv[0]);'
    text=sub_once(text,old,new,'capture t10')

    old='struct plan{native_state old{};ID3D11PixelShader *replacement_ps=nullptr;std::array<ID3D11ShaderResourceView *,2> replacement{};std::array<bool,2> replace{};bool exact_sampler=false;};'
    new='struct plan{native_state old{};ID3D11PixelShader *replacement_ps=nullptr;ID3D11ShaderResourceView *replacement_t10=nullptr;bool replace_t10=false;std::array<ID3D11ShaderResourceView *,2> replacement{};std::array<bool,2> replace{};bool exact_sampler=false;};'
    text=sub_once(text,old,new,'plan t10')

    old='p.replacement_ps=g_owned_dedicated_ps[owned_idx];if(p.replacement_ps==nullptr){++g_fail_open;return std::nullopt;}'
    new='p.replacement_ps=g_owned_dedicated_ps[owned_idx];if(p.replacement_ps==nullptr){++g_fail_open;return std::nullopt;}if(g_host_t10_candidate==nullptr||g_host_t10_candidate_material!=g_active_material){++g_t10_capture_miss;++g_fail_open;return std::nullopt;}p.replacement_t10=g_host_t10_candidate;g_host_t10_candidate=nullptr;g_host_t10_candidate_material=0;p.replace_t10=true;++g_t10_capture_use;'
    text=sub_once(text,old,new,'consume host t10')

    old='void apply(plan &p){if(p.replacement_ps){p.old.ctx->PSSetShader(p.replacement_ps,nullptr,0);++g_ps_dedicated_match;++g_owned_ps_bind;}for(int i=0;i<2;++i)'
    new='void apply(plan &p){if(p.replacement_ps){p.old.ctx->PSSetShader(p.replacement_ps,nullptr,0);++g_ps_dedicated_match;++g_owned_ps_bind;}if(p.replace_t10&&p.replacement_t10!=nullptr){auto *v=p.replacement_t10;p.old.ctx->PSSetShaderResources(10,1,&v);++g_t10_stitch_bind;}for(int i=0;i<2;++i)'
    text=sub_once(text,old,new,'apply t10')

    old='void restore(plan &p){for(int i=0;i<2;++i)'
    new='void restore(plan &p){if(p.replace_t10){auto *v=p.old.t10;p.old.ctx->PSSetShaderResources(10,1,&v);if(p.replacement_t10){p.replacement_t10->Release();p.replacement_t10=nullptr;}p.replace_t10=false;}for(int i=0;i<2;++i)'
    text=sub_once(text,old,new,'restore t10')

    old='if(!install_calls())reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V3.8] retail callsite guard failed; bridge fail-open");else reshade::log::message(reshade::log::level::info,"[DSRRL FULL ENVSPEC V3.8] native PS gate armed");return true;'
    new='const bool calls_ok=install_calls();if(!calls_ok)reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V3.9] retail callsite guard failed; bridge fail-open");const bool t10_ok=install_host_t10_stitch();if(!t10_ok)reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V3.9] build131 t10 stitch hook failed; PRESENT fail-open");else reshade::log::message(reshade::log::level::info,"[DSRRL FULL ENVSPEC V3.9] SpecRGB t10 transaction stitch armed");return true;'
    text=sub_once(text,old,new,'addon init hook')

    old='using namespace dsrrl::runtime_v2::full_envspec_v3;unpatch_calls();unregister_events();release_owned_dedicated();'
    new='using namespace dsrrl::runtime_v2::full_envspec_v3;uninstall_host_t10_stitch();unpatch_calls();unregister_events();release_owned_dedicated();'
    text=sub_once(text,old,new,'addon uninit hook')

    old='reshade::log::message(reshade::log::level::info,line);'
    new='reshade::log::message(reshade::log::level::info,line);char stitch[512]{};std::snprintf(stitch,sizeof(stitch),"[DSRRL V3.9 T10 STITCH] cap=%llu use=%llu miss=%llu bind=%llu pending=%u",static_cast<unsigned long long>(g_t10_capture_hit.load()),static_cast<unsigned long long>(g_t10_capture_use.load()),static_cast<unsigned long long>(g_t10_capture_miss.load()),static_cast<unsigned long long>(g_t10_stitch_bind.load()),g_host_t10_candidate!=nullptr?1u:0u);reshade::log::message(reshade::log::level::info,stitch);'
    text=sub_once(text,old,new,'telemetry stitch')

    text=text.replace('V3.8 Shader-Index Gate','V3.9 SpecRGB T10 Stitch')
    text=text.replace('[DSRRL FULL ENVSPEC V3.8]','[DSRRL FULL ENVSPEC V3.9]')
    text=text.replace('V3.8 exact shader-index P_Metal consumer island armed','V3.9 exact shader-index P_Metal consumer + SpecRGB t10 stitch armed')
    text=text.replace('Exact build131 + exact P_Metal + stock shader_index 894/913/932 -> owned DXBC72/73/74 consumer island; raw PTDE RGBA; native PS+t12/t14+s12/s14 transaction; fail-open elsewhere.',
                      'Exact build131 + exact P_Metal + stock shader_index 894/913/932 -> owned DXBC72/73/74; stitches build131 PTDE SpecRGB t10 into the same native draw transaction with raw PTDE t12/t14+s12/s14; full restore; fail-open elsewhere.')

    for tok in ('k_host_t10_bind_rva = 0x113B23u','g_host_t10_candidate_material','PSGetShaderResources(10,1,&s.t10)','PSSetShaderResources(10,1,&v)','V3.9 SpecRGB T10 Stitch','DSRRL V3.9 T10 STITCH'):
        if tok not in text: raise SystemExit('missing '+tok)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(text,encoding='utf-8')
    print('PASS V3.9 build131 SpecRGB t10 transaction stitch')

if __name__=='__main__': main()
