#!/usr/bin/env python3
from __future__ import annotations
import argparse,re
from pathlib import Path

INSERT = r'''
struct stock_identity{std::uint32_t receiver_id,size;std::array<std::uint8_t,16> checksum;};
inline constexpr std::array<stock_identity,3> k_stock_pmetal_hosts={{
    {33u,19872u,{{0xce,0x64,0x44,0xb8,0xc2,0x09,0xc0,0x6c,0x93,0x92,0x80,0xe0,0xdf,0x9b,0xee,0x21}}},
    {34u,19572u,{{0x6b,0xd7,0xb2,0xeb,0x87,0x2a,0x51,0xff,0x82,0x8b,0xa4,0x7f,0x46,0xe9,0xf7,0x64}}},
    {35u,18076u,{{0x07,0xbc,0x42,0x83,0x5c,0x0e,0x99,0x22,0x97,0x6c,0xec,0x42,0x87,0x1f,0x12,0x66}}}
}};
inline constexpr std::array<std::uintptr_t,3> k_dedicated_blob_rvas={0x18b000u,0x190250u,0x195370u};
std::array<ID3D11PixelShader *,3> g_owned_dedicated_ps{};
std::mutex g_owned_ps_mutex;
std::atomic<std::uint64_t> g_owned_ps_bind{0},g_owned_ps_create_fail{0};

bool stock_pmetal_host_code(const void *code,std::size_t size,std::uint32_t &receiver_id)noexcept
{
    if(code==nullptr||size<20)return false;const auto *b=static_cast<const std::uint8_t *>(code);if(std::memcmp(b,"DXBC",4)!=0)return false;
    for(const auto &r:k_stock_pmetal_hosts)if(r.size==size&&std::memcmp(b+4,r.checksum.data(),16)==0){receiver_id=r.receiver_id;return true;}return false;
}

bool validate_dedicated_blob_seh(const std::uint8_t *code,std::uint32_t expected_size,const std::uint8_t *checksum)noexcept
{
    if(code==nullptr||checksum==nullptr)return false;
    __try{
        return std::memcmp(code,"DXBC",4)==0&&
               *reinterpret_cast<const std::uint32_t *>(code+24)==expected_size&&
               std::memcmp(code+4,checksum,16)==0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}

bool ensure_owned_dedicated(ID3D11Device *dev)
{
    if(dev==nullptr)return false;std::lock_guard lock(g_owned_ps_mutex);
    if(g_owned_dedicated_ps[0]&&g_owned_dedicated_ps[1]&&g_owned_dedicated_ps[2])return true;
    for(auto *&p:g_owned_dedicated_ps){if(p){p->Release();p=nullptr;}}
    HMODULE host=GetModuleHandleW(L"DSRRL_Material_Response_1.45.addon64");if(host==nullptr){++g_owned_ps_create_fail;return false;}
    const auto base=reinterpret_cast<std::uintptr_t>(host);
    for(std::size_t i=0;i<3;++i){
        const auto *code=reinterpret_cast<const std::uint8_t *>(base+k_dedicated_blob_rvas[i]);
        if(!validate_dedicated_blob_seh(code,k_dedicated[i].size,k_dedicated[i].checksum.data())){++g_owned_ps_create_fail;return false;}
        if(FAILED(dev->CreatePixelShader(code,k_dedicated[i].size,nullptr,&g_owned_dedicated_ps[i]))||g_owned_dedicated_ps[i]==nullptr){++g_owned_ps_create_fail;for(auto *&p:g_owned_dedicated_ps){if(p){p->Release();p=nullptr;}}return false;}
    }
    g_ps_dedicated_created.store(3,std::memory_order_relaxed);return true;
}

void release_owned_dedicated()noexcept{std::lock_guard lock(g_owned_ps_mutex);for(auto *&p:g_owned_dedicated_ps){if(p){p->Release();p=nullptr;}}}
'''.strip()

INIT_PIPELINE = r'''
void init_pipeline_v34(reshade::api::device *,reshade::api::pipeline_layout,std::uint32_t count,const reshade::api::pipeline_subobject *subs,reshade::api::pipeline pipeline)
{
    const reshade::api::shader_desc *ps=nullptr;for(std::uint32_t i=0;i<count;++i)if(subs[i].type==reshade::api::pipeline_subobject_type::pixel_shader){ps=static_cast<const reshade::api::shader_desc *>(subs[i].data);break;}
    if(ps==nullptr||ps->code==nullptr||ps->code_size==0)return;std::uint32_t id=0;receiver_rec rec{};
    if(stock_pmetal_host_code(ps->code,ps->code_size,id)){std::uint64_t family=0;std::uint32_t ignored=0;if(!classify_receiver(ps->code,ps->code_size,0,&ignored,&family))return;rec={receiver_kind::stock,id};++g_ps_stock_created;}
    else{std::uint64_t family=0;if(!classify_receiver(ps->code,ps->code_size,0,&id,&family))return;rec={receiver_kind::stock,id};++g_ps_stock_created;}
    std::lock_guard lock(g_mutex);g_ps[pipeline.handle]=rec;
}
'''.strip()

STATE = r'''
struct native_state{
    ID3D11DeviceContext *ctx=nullptr;ID3D11PixelShader *ps=nullptr;std::array<ID3D11ShaderResourceView *,2> srv{};std::array<ID3D11SamplerState *,2> sampler{};
    native_state()=default;native_state(const native_state&)=delete;native_state&operator=(const native_state&)=delete;
    native_state(native_state&&o)noexcept:ctx(o.ctx),ps(o.ps),srv(o.srv),sampler(o.sampler){o.ctx=nullptr;o.ps=nullptr;o.srv={};o.sampler={};}
    native_state&operator=(native_state&&o)noexcept{if(this!=&o){release();ctx=o.ctx;ps=o.ps;srv=o.srv;sampler=o.sampler;o.ctx=nullptr;o.ps=nullptr;o.srv={};o.sampler={};}return *this;}
    ~native_state(){release();}void release()noexcept{if(ps){ps->Release();ps=nullptr;}for(auto *&v:srv)if(v){v->Release();v=nullptr;}for(auto *&s:sampler)if(s){s->Release();s=nullptr;}ctx=nullptr;}
};
std::optional<native_state> capture(reshade::api::command_list *cmd){if(cmd==nullptr||cmd->get_native()==0)return std::nullopt;native_state s{};s.ctx=reinterpret_cast<ID3D11DeviceContext *>(static_cast<std::uintptr_t>(cmd->get_native()));s.ctx->PSGetShader(&s.ps,nullptr,nullptr);s.ctx->PSGetShaderResources(12,1,&s.srv[0]);s.ctx->PSGetShaderResources(14,1,&s.srv[1]);s.ctx->PSGetSamplers(12,1,&s.sampler[0]);s.ctx->PSGetSamplers(14,1,&s.sampler[1]);return std::optional<native_state>(std::move(s));}

struct plan{native_state old{};ID3D11PixelShader *replacement_ps=nullptr;std::array<ID3D11ShaderResourceView *,2> replacement{};std::array<bool,2> replace{};bool exact_sampler=false;};
'''.strip()

BUILD_PLAN = r'''
std::optional<plan> build_plan_v34(reshade::api::command_list *cmd)
{
    const auto b=active_binding();if(!b.has_value()||b->record==nullptr){++g_material_unknown;++g_fail_open;return std::nullopt;}const material_record &m=*b->record;const auto r=native_receiver(cmd);if(!r.has_value()){++g_receiver_reject;++g_fail_open;return std::nullopt;}++g_receiver_match;
    auto st=capture(cmd);if(!st.has_value()){++g_fail_open;return std::nullopt;}plan p{};p.old=std::move(*st);
    if(m.state==material_envspec_state::present){
        if(!exact_pmetal(m)){++g_present_nonpmetal;++g_material_unknown;++g_fail_open;return std::nullopt;}
        if(r->kind!=receiver_kind::stock||r->id<33u||r->id>35u){++g_present_reject;++g_fail_open;return std::nullopt;}
        ID3D11Device *dev=nullptr;p.old.ctx->GetDevice(&dev);const bool own_ok=ensure_owned_dedicated(dev);if(dev)dev->Release();if(!own_ok){++g_fail_open;return std::nullopt;}
        p.replacement_ps=g_owned_dedicated_ps[r->id-33u];if(p.replacement_ps==nullptr){++g_fail_open;return std::nullopt;}
        if(p.old.srv[0]==nullptr||p.old.srv[1]==nullptr){++g_resource_route_miss;++g_fail_open;return std::nullopt;}
        const auto a=probe_for_view(reshade::api::resource_view{static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p.old.srv[0]))});const auto bb=probe_for_view(reshade::api::resource_view{static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p.old.srv[1]))});if(!a.has_value()||!bb.has_value()){++g_resource_route_miss;++g_fail_open;return std::nullopt;}
        const auto ra=raw_cube(*a,m.slot),rb=raw_cube(*bb,m.slot);if(!ra.has_value()||!rb.has_value()){++g_resource_route_miss;++g_fail_open;return std::nullopt;}p.replacement[0]=reinterpret_cast<ID3D11ShaderResourceView *>(static_cast<std::uintptr_t>(ra->handle));p.replacement[1]=reinterpret_cast<ID3D11ShaderResourceView *>(static_cast<std::uintptr_t>(rb->handle));p.replace={true,true};p.exact_sampler=true;g_resource_route_hit.fetch_add(2);++g_material_present;return p;
    }
    if(m.state==material_envspec_state::explicit_none&&b->explicit_none_safe){if(r->kind!=receiver_kind::stock){++g_fail_open;return std::nullopt;}if(g_black_view.handle==0&&!create_black_cube()){++g_fail_open;return std::nullopt;}auto *black=reinterpret_cast<ID3D11ShaderResourceView *>(static_cast<std::uintptr_t>(g_black_view.handle));bool any=false;for(int i=0;i<2;++i){if(p.old.srv[i]==nullptr)continue;p.replacement[i]=black;p.replace[i]=true;any=true;}if(!any){++g_resource_route_miss;++g_fail_open;return std::nullopt;}++g_material_explicit_none;return p;}
    ++g_material_unknown;++g_fail_open;return std::nullopt;
}

void apply(plan &p){if(p.replacement_ps){p.old.ctx->PSSetShader(p.replacement_ps,nullptr,0);++g_ps_dedicated_match;++g_owned_ps_bind;}for(int i=0;i<2;++i){if(!p.replace[i])continue;const UINT slot=i==0?12u:14u;auto *v=p.replacement[i];p.old.ctx->PSSetShaderResources(slot,1,&v);if(slot==12)++g_t12_rebind;else++g_t14_rebind;if(p.exact_sampler&&g_ptde_sampler.handle!=0){auto *s=reinterpret_cast<ID3D11SamplerState *>(static_cast<std::uintptr_t>(g_ptde_sampler.handle));p.old.ctx->PSSetSamplers(slot,1,&s);++g_sampler_rebind;}}++g_native_tx;}
void restore(plan &p){for(int i=0;i<2;++i){if(!p.replace[i])continue;const UINT slot=i==0?12u:14u;auto *v=p.old.srv[i];p.old.ctx->PSSetShaderResources(slot,1,&v);auto *s=p.old.sampler[i];p.old.ctx->PSSetSamplers(slot,1,&s);}if(p.replacement_ps)p.old.ctx->PSSetShader(p.old.ps,nullptr,0);++g_restore_pass;}
'''.strip()

def sub_once(text,pat,repl,label):
    out,n=re.subn(pat,repl,text,count=1,flags=re.S)
    if n!=1:raise SystemExit(f'{label} replacement count={n}')
    return out

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--input',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    text=a.input.read_text(encoding='utf-8')
    anchor='std::unordered_map<std::uint64_t,receiver_rec> g_ps;'
    if anchor not in text:raise SystemExit('g_ps anchor missing')
    text=text.replace(anchor,anchor+'\n\n'+INSERT,1)
    text=sub_once(text,r'void init_pipeline_v34\(.*?\n\}\n(?=void destroy_pipeline_v34)',INIT_PIPELINE+'\n','init_pipeline')
    text=sub_once(text,r'struct native_state\{.*?struct plan\{.*?\};\n',STATE+'\n','native_state/plan')
    text=sub_once(text,r'std::optional<plan> build_plan_v34\(.*?\nvoid restore\(plan &p\)\{.*?\}\n',BUILD_PLAN+'\n','build_plan/apply/restore')
    text=text.replace('V3.4 Native PS Gate','V3.6 Owned Dedicated PS')
    text=text.replace('[DSRRL FULL ENVSPEC V3.4]','[DSRRL FULL ENVSPEC V3.6]')
    text=text.replace('V3.4 native PS gate armed','V3.6 owned dedicated P_Metal PS island armed')
    text=text.replace('Exact build131 + actual material + native D3D11 PS receiver gate; raw PTDE RGBA; native t12/t14+s12/s14 transaction; fail-open elsewhere.','Exact build131 + exact P_Metal + exact stock 33/34/35 -> owned DXBC72/73/74 consumer island; raw PTDE RGBA; native PS+t12/t14+s12/s14 transaction; fail-open elsewhere.')
    old='using namespace dsrrl::runtime_v2::full_envspec_v3;unpatch_calls();unregister_events();{std::lock_guard lock(g_mutex);'
    new='using namespace dsrrl::runtime_v2::full_envspec_v3;unpatch_calls();unregister_events();release_owned_dedicated();{std::lock_guard lock(g_mutex);'
    if old not in text:raise SystemExit('uninit anchor missing')
    text=text.replace(old,new,1)
    for tok in ('0x18b000u,0x190250u,0x195370u','CreatePixelShader','r->id<33u||r->id>35u','PSSetShader(p.replacement_ps','r8g8b8a8_unorm','V3.6 Owned Dedicated PS'):
        if tok not in text:raise SystemExit('missing '+tok)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(text,encoding='utf-8');print('PASS V3.6 owned dedicated PS island')
if __name__=='__main__':main()
