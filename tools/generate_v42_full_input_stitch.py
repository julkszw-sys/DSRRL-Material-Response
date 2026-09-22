#!/usr/bin/env python3
from __future__ import annotations
import argparse
from pathlib import Path

STITCH = r'''
constexpr std::uintptr_t k_host_t10_bind_rva = 0x113B23u;
constexpr std::array<std::uint8_t,k_detour_stolen> k_host_t10_bind_prolog = {
    0x48,0x83,0xEC,0x28,0x48,0x8B,0x01,0x4C,0x8D,0x4C,0x24,0x20,0x49,0x89,0x11};
detour_state g_host_t10_detour;
using host_t10_bind_fn = void (__fastcall *)(ID3D11DeviceContext *,ID3D11ShaderResourceView *);
host_t10_bind_fn g_host_t10_original = nullptr;
thread_local ID3D11ShaderResourceView *g_host_t10_candidate = nullptr;
thread_local std::uintptr_t g_host_t10_candidate_material = 0;
thread_local ID3D11Buffer *g_host_b12_candidate = nullptr;
thread_local std::uintptr_t g_host_b12_candidate_material = 0;
std::atomic<std::uint64_t> g_t10_capture_hit{0},g_t10_capture_use{0},g_t10_capture_miss{0},g_t10_stitch_bind{0};
std::atomic<std::uint64_t> g_b12_capture_hit{0},g_b12_capture_use{0},g_b12_capture_miss{0},g_b12_stitch_bind{0};

void clear_host_input_candidates() noexcept
{
    if(g_host_t10_candidate!=nullptr){g_host_t10_candidate->Release();g_host_t10_candidate=nullptr;}
    g_host_t10_candidate_material=0;
    if(g_host_b12_candidate!=nullptr){g_host_b12_candidate->Release();g_host_b12_candidate=nullptr;}
    g_host_b12_candidate_material=0;
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
        if(g_host_t10_candidate_material!=material){
            if(g_host_t10_candidate!=nullptr){g_host_t10_candidate->Release();g_host_t10_candidate=nullptr;}
            g_host_t10_candidate_material=0;
        }
        if(g_host_t10_candidate==nullptr){srv->AddRef();g_host_t10_candidate=srv;g_host_t10_candidate_material=material;++g_t10_capture_hit;}
    }
    if(g_host_t10_original!=nullptr)g_host_t10_original(ctx,srv);
}

void observe_host_b12(
    reshade::api::command_list *,
    reshade::api::shader_stage stages,
    reshade::api::pipeline_layout,
    std::uint32_t,
    const reshade::api::descriptor_table_update &update)
{
    if((stages&reshade::api::shader_stage::pixel)!=reshade::api::shader_stage::pixel)return;
    if(update.type!=reshade::api::descriptor_type::constant_buffer||update.count==0||update.descriptors==nullptr)return;
    const auto material=g_active_material;
    if(material==0||!active_material_is_exact_pmetal())return;
    const auto *ranges=static_cast<const reshade::api::buffer_range *>(update.descriptors);
    for(std::uint32_t i=0;i<update.count;++i){
        if(update.binding+i!=12u)continue;
        auto *buffer=reinterpret_cast<ID3D11Buffer *>(static_cast<std::uintptr_t>(ranges[i].buffer.handle));
        if(buffer==nullptr)return;
        if(g_host_b12_candidate_material!=material){
            if(g_host_b12_candidate!=nullptr){g_host_b12_candidate->Release();g_host_b12_candidate=nullptr;}
            g_host_b12_candidate_material=0;
        }
        if(g_host_b12_candidate==nullptr){buffer->AddRef();g_host_b12_candidate=buffer;g_host_b12_candidate_material=material;++g_b12_capture_hit;}
        return;
    }
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

void uninstall_host_input_stitch() noexcept
{
    clear_host_input_candidates();
    uninstall_detour(g_host_t10_detour);
    g_host_t10_original=nullptr;
}
'''.strip()


def one(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f'{label}: anchor missing')
    return text.replace(old,new,1)


def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--input',type=Path,required=True); ap.add_argument('--output',type=Path,required=True); a=ap.parse_args()
    text=a.input.read_text(encoding='utf-8')

    # Selector is the per-draw semantic boundary. Clear stale captured host inputs before a new actual_material is installed.
    decl='selector_fn g_selector_entry=nullptr;'
    text=one(text,decl,decl+'\nvoid clear_host_input_candidates() noexcept;', 'forward declaration')
    old='if(actual!=0)++g_selector_resolved; g_active_material=actual; if(g_selector_entry==nullptr)return 0;'
    new='if(actual!=0)++g_selector_resolved; clear_host_input_candidates(); g_active_material=actual; if(g_selector_entry==nullptr)return 0;'
    text=one(text,old,new,'selector clear')

    anchor='enum class receiver_kind:std::uint8_t{stock,dedicated}; struct receiver_rec{receiver_kind kind;std::uint32_t id;};'
    text=one(text,anchor,STITCH+'\n\n'+anchor,'stitch insertion')

    old='ID3D11DeviceContext *ctx=nullptr;ID3D11PixelShader *ps=nullptr;std::array<ID3D11ShaderResourceView *,2> srv{};std::array<ID3D11SamplerState *,2> sampler{};'
    new='ID3D11DeviceContext *ctx=nullptr;ID3D11PixelShader *ps=nullptr;ID3D11ShaderResourceView *t10=nullptr;ID3D11Buffer *b12=nullptr;std::array<ID3D11ShaderResourceView *,2> srv{};std::array<ID3D11SamplerState *,2> sampler{};'
    text=one(text,old,new,'native state fields')
    old='native_state(native_state&&o)noexcept:ctx(o.ctx),ps(o.ps),srv(o.srv),sampler(o.sampler){o.ctx=nullptr;o.ps=nullptr;o.srv={};o.sampler={};}'
    new='native_state(native_state&&o)noexcept:ctx(o.ctx),ps(o.ps),t10(o.t10),b12(o.b12),srv(o.srv),sampler(o.sampler){o.ctx=nullptr;o.ps=nullptr;o.t10=nullptr;o.b12=nullptr;o.srv={};o.sampler={};}'
    text=one(text,old,new,'move ctor')
    old='native_state&operator=(native_state&&o)noexcept{if(this!=&o){release();ctx=o.ctx;ps=o.ps;srv=o.srv;sampler=o.sampler;o.ctx=nullptr;o.ps=nullptr;o.srv={};o.sampler={};}return *this;}'
    new='native_state&operator=(native_state&&o)noexcept{if(this!=&o){release();ctx=o.ctx;ps=o.ps;t10=o.t10;b12=o.b12;srv=o.srv;sampler=o.sampler;o.ctx=nullptr;o.ps=nullptr;o.t10=nullptr;o.b12=nullptr;o.srv={};o.sampler={};}return *this;}'
    text=one(text,old,new,'move assign')
    old='~native_state(){release();}void release()noexcept{if(ps){ps->Release();ps=nullptr;}for(auto *&v:srv)if(v){v->Release();v=nullptr;}for(auto *&s:sampler)if(s){s->Release();s=nullptr;}ctx=nullptr;}'
    new='~native_state(){release();}void release()noexcept{if(ps){ps->Release();ps=nullptr;}if(t10){t10->Release();t10=nullptr;}if(b12){b12->Release();b12=nullptr;}for(auto *&v:srv)if(v){v->Release();v=nullptr;}for(auto *&s:sampler)if(s){s->Release();s=nullptr;}ctx=nullptr;}'
    text=one(text,old,new,'release')
    old='s.ctx->PSGetShader(&s.ps,nullptr,nullptr);s.ctx->PSGetShaderResources(12,1,&s.srv[0]);'
    new='s.ctx->PSGetShader(&s.ps,nullptr,nullptr);s.ctx->PSGetShaderResources(10,1,&s.t10);s.ctx->PSGetConstantBuffers(12,1,&s.b12);s.ctx->PSGetShaderResources(12,1,&s.srv[0]);'
    text=one(text,old,new,'capture inputs')

    old='struct plan{native_state old{};ID3D11PixelShader *replacement_ps=nullptr;std::array<ID3D11ShaderResourceView *,2> replacement{};std::array<bool,2> replace{};bool exact_sampler=false;};'
    new='struct plan{native_state old{};ID3D11PixelShader *replacement_ps=nullptr;ID3D11ShaderResourceView *replacement_t10=nullptr;ID3D11Buffer *replacement_b12=nullptr;bool replace_t10=false;bool replace_b12=false;std::array<ID3D11ShaderResourceView *,2> replacement{};std::array<bool,2> replace{};bool exact_sampler=false;};'
    text=one(text,old,new,'plan inputs')

    old='p.replacement_ps=g_owned_dedicated_ps[owned_idx];if(p.replacement_ps==nullptr){++g_fail_open;return std::nullopt;}'
    new='''p.replacement_ps=g_owned_dedicated_ps[owned_idx];if(p.replacement_ps==nullptr){++g_fail_open;return std::nullopt;}
        if(g_host_t10_candidate==nullptr||g_host_t10_candidate_material!=g_active_material){++g_t10_capture_miss;++g_fail_open;return std::nullopt;}
        if(g_host_b12_candidate==nullptr||g_host_b12_candidate_material!=g_active_material){++g_b12_capture_miss;++g_fail_open;return std::nullopt;}
        p.replacement_t10=g_host_t10_candidate;g_host_t10_candidate=nullptr;g_host_t10_candidate_material=0;p.replace_t10=true;++g_t10_capture_use;
        p.replacement_b12=g_host_b12_candidate;g_host_b12_candidate=nullptr;g_host_b12_candidate_material=0;p.replace_b12=true;++g_b12_capture_use;'''
    text=one(text,old,new,'consume host inputs')

    old='void apply(plan &p){if(p.replacement_ps){p.old.ctx->PSSetShader(p.replacement_ps,nullptr,0);++g_ps_dedicated_match;++g_owned_ps_bind;}for(int i=0;i<2;++i)'
    new='void apply(plan &p){if(p.replacement_ps){p.old.ctx->PSSetShader(p.replacement_ps,nullptr,0);++g_ps_dedicated_match;++g_owned_ps_bind;}if(p.replace_t10&&p.replacement_t10!=nullptr){auto *v=p.replacement_t10;p.old.ctx->PSSetShaderResources(10,1,&v);++g_t10_stitch_bind;}if(p.replace_b12&&p.replacement_b12!=nullptr){auto *v=p.replacement_b12;p.old.ctx->PSSetConstantBuffers(12,1,&v);++g_b12_stitch_bind;}for(int i=0;i<2;++i)'
    text=one(text,old,new,'apply inputs')
    old='void restore(plan &p){for(int i=0;i<2;++i)'
    new='void restore(plan &p){if(p.replace_t10){auto *v=p.old.t10;p.old.ctx->PSSetShaderResources(10,1,&v);if(p.replacement_t10){p.replacement_t10->Release();p.replacement_t10=nullptr;}p.replace_t10=false;}if(p.replace_b12){auto *v=p.old.b12;p.old.ctx->PSSetConstantBuffers(12,1,&v);if(p.replacement_b12){p.replacement_b12->Release();p.replacement_b12=nullptr;}p.replace_b12=false;}for(int i=0;i<2;++i)'
    text=one(text,old,new,'restore inputs')

    old='void register_events(){reshade::register_event<reshade::addon_event::init_device>(on_init_device);'
    new='void register_events(){reshade::register_event<reshade::addon_event::push_descriptors>(observe_host_b12);reshade::register_event<reshade::addon_event::init_device>(on_init_device);'
    text=one(text,old,new,'register b12 observer')
    old='void unregister_events(){reshade::unregister_event<reshade::addon_event::present>(present_v34);'
    new='void unregister_events(){reshade::unregister_event<reshade::addon_event::push_descriptors>(observe_host_b12);reshade::unregister_event<reshade::addon_event::present>(present_v34);'
    text=one(text,old,new,'unregister b12 observer')

    old='if(!install_calls())reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V4.1] retail callsite guard failed; bridge fail-open");else reshade::log::message(reshade::log::level::info,"[DSRRL FULL ENVSPEC V4.1] native PS gate armed");return true;'
    new='const bool calls_ok=install_calls();if(!calls_ok)reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V4.2] retail callsite guard failed; bridge fail-open");const bool t10_ok=install_host_t10_stitch();if(!t10_ok)reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V4.2] build131 t10 stitch hook failed; PRESENT fail-open");else reshade::log::message(reshade::log::level::info,"[DSRRL FULL ENVSPEC V4.2] full t10+b12 input stitch armed");return true;'
    text=one(text,old,new,'addon init')
    old='using namespace dsrrl::runtime_v2::full_envspec_v3;unpatch_calls();unregister_events();release_owned_dedicated();'
    new='using namespace dsrrl::runtime_v2::full_envspec_v3;uninstall_host_input_stitch();unpatch_calls();unregister_events();release_owned_dedicated();'
    text=one(text,old,new,'addon uninit')

    old='reshade::log::message(reshade::log::level::info,line);'
    new='reshade::log::message(reshade::log::level::info,line);char stitch[640]{};std::snprintf(stitch,sizeof(stitch),"[DSRRL V4.2 INPUT STITCH] t10 cap=%llu use=%llu miss=%llu bind=%llu | b12 cap=%llu use=%llu miss=%llu bind=%llu pending=%u/%u",static_cast<unsigned long long>(g_t10_capture_hit.load()),static_cast<unsigned long long>(g_t10_capture_use.load()),static_cast<unsigned long long>(g_t10_capture_miss.load()),static_cast<unsigned long long>(g_t10_stitch_bind.load()),static_cast<unsigned long long>(g_b12_capture_hit.load()),static_cast<unsigned long long>(g_b12_capture_use.load()),static_cast<unsigned long long>(g_b12_capture_miss.load()),static_cast<unsigned long long>(g_b12_stitch_bind.load()),g_host_t10_candidate!=nullptr?1u:0u,g_host_b12_candidate!=nullptr?1u:0u);reshade::log::message(reshade::log::level::info,stitch);'
    text=one(text,old,new,'telemetry')

    text=text.replace('V4.1 Preserve-r1.x EnvSpec','V4.2 Full Input Stitch EnvSpec')
    text=text.replace('[DSRRL FULL ENVSPEC V4.1]','[DSRRL FULL ENVSPEC V4.2]')
    text=text.replace('V4.1 P_Metal EnvSpec island armed with live r1.x preserved','V4.2 P_Metal EnvSpec island armed with t10+b12 input contract')
    text=text.replace('Exact build131 + exact P_Metal + stock shader_index 894/913/932 -> SHA-guarded stock-body-plus-PTDE-EnvSpec operator island; raw PTDE RGBA; native PS+t12/t14+s12/s14 transaction; no legacy material-tail changes; fail-open elsewhere.',
                      'Exact build131 + exact P_Metal + shader_index 894/913/932 -> SHA-guarded V4.1 operator island; stitches host-selected PTDE SpecRGB t10 and PTDE donor b12 into the same native PS+t12/t14+s12/s14 transaction; full restore; fail-open elsewhere.')

    required=('k_host_t10_bind_rva = 0x113B23u','descriptor_type::constant_buffer','PSGetConstantBuffers(12,1,&s.b12)','PSSetConstantBuffers(12,1,&v)','PSSetShaderResources(10,1,&v)','DSRRL V4.2 INPUT STITCH','V4.2 Full Input Stitch EnvSpec')
    for tok in required:
        if tok not in text: raise SystemExit('missing '+tok)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(text,encoding='utf-8')
    print('PASS V4.2 full host t10+b12 input-contract stitch')

if __name__=='__main__': main()
