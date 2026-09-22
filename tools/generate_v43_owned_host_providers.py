#!/usr/bin/env python3
from __future__ import annotations
import argparse,re
from pathlib import Path

PROVIDERS = r'''
constexpr std::uintptr_t k_host_spec_state_rva = 0x113308u;
constexpr std::uintptr_t k_host_spec_prepare_rva = 0x11350Cu;
constexpr std::uintptr_t k_host_spec_bind_rva = 0x113B45u;
constexpr std::uintptr_t k_host_b12_provider_rva = 0x77A0u;
constexpr std::uintptr_t k_host_b12_context_rva = 0x1070C0u;
constexpr std::int32_t k_exact_pmetal_route = 345;
constexpr std::size_t k_host_spec_path_wchars = 0x208u;

using host_spec_state_fn = void * (__fastcall *)();
using host_spec_prepare_fn = void (__fastcall *)(ID3D11DeviceContext *);
using host_spec_bind_fn = void (__fastcall *)(ID3D11DeviceContext *);
using host_b12_provider_fn = ID3D11Buffer * (__fastcall *)(void *,std::int32_t);
host_spec_state_fn g_host_spec_state = nullptr;
host_spec_prepare_fn g_host_spec_prepare = nullptr;
host_spec_bind_fn g_host_spec_bind = nullptr;
host_b12_provider_fn g_host_b12_provider = nullptr;
void *g_host_b12_context = nullptr;

std::atomic<std::uint64_t> g_v43_t10_hit{0},g_v43_t10_miss{0};
std::atomic<std::uint64_t> g_v43_b12_hit{0},g_v43_b12_miss{0};
std::atomic<std::uint64_t> g_v43_provider_guard_pass{0},g_v43_provider_guard_fail{0};
std::atomic<std::uint64_t> g_v43_spec_path_canonical{0},g_v43_spec_path_legacy{0},g_v43_spec_path_missing{0};
std::atomic<std::uint64_t> g_t10_stitch_bind{0},g_b12_stitch_bind{0};

void clear_host_input_candidates() noexcept {}

bool bytes_are(const std::uint8_t *p,const std::initializer_list<std::uint8_t> &v) noexcept
{
    if(p==nullptr)return false;std::size_t i=0;for(const auto b:v){if(p[i++]!=b)return false;}return true;
}

bool resolve_host_owned_providers()
{
    HMODULE host=GetModuleHandleW(L"DSRRL_Material_Response_1.45.addon64");
    if(host==nullptr){++g_v43_provider_guard_fail;return false;}
    auto *base=reinterpret_cast<std::uint8_t *>(host);
    const bool guards=
        bytes_are(base+k_host_spec_state_rva,{0x48,0x83,0xEC,0x28,0xE8,0xEF,0xFC,0xFF})&&
        bytes_are(base+k_host_spec_prepare_rva,{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x55,0x53})&&
        bytes_are(base+k_host_spec_bind_rva,{0x56,0x57,0x48,0x83,0xEC,0x28,0x48,0x89})&&
        bytes_are(base+k_host_b12_provider_rva,{0x48,0x89,0x5C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x55,0x57,0x41,0x54,0x41});
    if(!guards){++g_v43_provider_guard_fail;return false;}
    g_host_spec_state=reinterpret_cast<host_spec_state_fn>(base+k_host_spec_state_rva);
    g_host_spec_prepare=reinterpret_cast<host_spec_prepare_fn>(base+k_host_spec_prepare_rva);
    g_host_spec_bind=reinterpret_cast<host_spec_bind_fn>(base+k_host_spec_bind_rva);
    g_host_b12_provider=reinterpret_cast<host_b12_provider_fn>(base+k_host_b12_provider_rva);
    g_host_b12_context=base+k_host_b12_context_rva;
    ++g_v43_provider_guard_pass;return true;
}

bool directory_has_dds(const std::filesystem::path &p) noexcept
{
    std::error_code ec;if(!std::filesystem::is_directory(p,ec)||ec)return false;
    for(std::filesystem::directory_iterator it(p,ec),end;!ec&&it!=end;it.increment(ec)){
        if(ec)break;if(!it->is_regular_file(ec)||ec)continue;
        auto ext=it->path().extension().wstring();for(auto &c:ext)c=static_cast<wchar_t>(std::towlower(c));
        if(ext==L".dds")return true;
    }
    return false;
}

bool ensure_host_spec_namespace() noexcept
{
    if(g_host_spec_state==nullptr)return false;
    void *state=nullptr;__try{state=g_host_spec_state();}__except(EXCEPTION_EXECUTE_HANDLER){state=nullptr;}
    if(state==nullptr)return false;
    HMODULE host=GetModuleHandleW(L"DSRRL_Material_Response_1.45.addon64");if(host==nullptr)return false;
    const auto root=module_path(host).parent_path();
    const std::array<std::filesystem::path,3> candidates={root/L"DSRRL"/L"Specular",root/L"DSRRL_PTDE_SPEC",root/L"DSRRL"/L"PTDE_SPEC"};
    std::filesystem::path chosen{};std::size_t which=0;
    for(std::size_t i=0;i<candidates.size();++i)if(directory_has_dds(candidates[i])){chosen=candidates[i];which=i;break;}
    if(chosen.empty()){++g_v43_spec_path_missing;return false;}
    std::wstring s=chosen.wstring();if(s.empty())return false;if(s.back()!=L'\\'&&s.back()!=L'/')s.push_back(L'\\');
    if(s.size()+1>=k_host_spec_path_wchars)return false;
    __try{
        auto *dst=reinterpret_cast<wchar_t *>(static_cast<std::uint8_t *>(state)+0xB38u);
        std::memset(dst,0,k_host_spec_path_wchars*sizeof(wchar_t));
        std::memcpy(dst,s.data(),s.size()*sizeof(wchar_t));
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    if(which==0)++g_v43_spec_path_canonical;else ++g_v43_spec_path_legacy;
    return true;
}

bool acquire_owned_host_t10(ID3D11DeviceContext *ctx,ID3D11ShaderResourceView **out) noexcept
{
    if(out==nullptr)return false;*out=nullptr;
    if(ctx==nullptr||g_host_spec_prepare==nullptr||g_host_spec_bind==nullptr||!ensure_host_spec_namespace()){++g_v43_t10_miss;return false;}
    ID3D11ShaderResourceView *prior=nullptr;ctx->PSGetShaderResources(10,1,&prior);
    __try{g_host_spec_prepare(ctx);}__except(EXCEPTION_EXECUTE_HANDLER){if(prior)prior->Release();++g_v43_t10_miss;return false;}
    ID3D11ShaderResourceView *nullv=nullptr;ctx->PSSetShaderResources(10,1,&nullv);
    __try{g_host_spec_bind(ctx);}__except(EXCEPTION_EXECUTE_HANDLER){}
    ID3D11ShaderResourceView *candidate=nullptr;ctx->PSGetShaderResources(10,1,&candidate);
    ctx->PSSetShaderResources(10,1,&prior);if(prior)prior->Release();
    if(candidate==nullptr){++g_v43_t10_miss;return false;}
    *out=candidate;++g_v43_t10_hit;return true;
}

ID3D11Buffer *acquire_owned_host_b12() noexcept
{
    if(g_host_b12_provider==nullptr||g_host_b12_context==nullptr){++g_v43_b12_miss;return nullptr;}
    ID3D11Buffer *buffer=nullptr;__try{buffer=g_host_b12_provider(g_host_b12_context,k_exact_pmetal_route);}__except(EXCEPTION_EXECUTE_HANDLER){buffer=nullptr;}
    if(buffer==nullptr){++g_v43_b12_miss;return nullptr;}
    D3D11_BUFFER_DESC d{};buffer->GetDesc(&d);
    if(d.ByteWidth<64u||(d.BindFlags&D3D11_BIND_CONSTANT_BUFFER)==0u){buffer->Release();++g_v43_b12_miss;return nullptr;}
    ++g_v43_b12_hit;return buffer;
}
'''.strip()


def one(text: str, old: str, new: str, label: str) -> str:
    if old not in text: raise SystemExit(f'{label}: anchor missing')
    return text.replace(old,new,1)


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--input',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    text=a.input.read_text(encoding='utf-8')

    # Remove the V4.2 passive host-listener implementation entirely. V4.3 owns provider acquisition.
    pattern=r'constexpr std::uintptr_t k_host_t10_bind_rva = 0x113B23u;.*?void uninstall_host_input_stitch\(\) noexcept\s*\{.*?\}\s*(?=enum class receiver_kind)'
    text,n=re.subn(pattern,PROVIDERS+'\n\n',text,count=1,flags=re.S)
    if n!=1: raise SystemExit(f'passive stitch block replacement count={n}')

    old='''p.replacement_ps=g_owned_dedicated_ps[owned_idx];if(p.replacement_ps==nullptr){++g_fail_open;return std::nullopt;}
        if(g_host_t10_candidate==nullptr||g_host_t10_candidate_material!=g_active_material){++g_t10_capture_miss;++g_fail_open;return std::nullopt;}
        if(g_host_b12_candidate==nullptr||g_host_b12_candidate_material!=g_active_material){++g_b12_capture_miss;++g_fail_open;return std::nullopt;}
        p.replacement_t10=g_host_t10_candidate;g_host_t10_candidate=nullptr;g_host_t10_candidate_material=0;p.replace_t10=true;++g_t10_capture_use;
        p.replacement_b12=g_host_b12_candidate;g_host_b12_candidate=nullptr;g_host_b12_candidate_material=0;p.replace_b12=true;++g_b12_capture_use;'''
    new='''p.replacement_ps=g_owned_dedicated_ps[owned_idx];if(p.replacement_ps==nullptr){++g_fail_open;return std::nullopt;}
        ID3D11ShaderResourceView *owned_t10=nullptr;if(!acquire_owned_host_t10(p.old.ctx,&owned_t10)){++g_fail_open;return std::nullopt;}
        ID3D11Buffer *owned_b12=acquire_owned_host_b12();if(owned_b12==nullptr){owned_t10->Release();++g_fail_open;return std::nullopt;}
        p.replacement_t10=owned_t10;p.replace_t10=true;
        p.replacement_b12=owned_b12;p.replace_b12=true;'''
    text=one(text,old,new,'owned provider consume')

    text=one(text,'void register_events(){reshade::register_event<reshade::addon_event::push_descriptors>(observe_host_b12);reshade::register_event<reshade::addon_event::init_device>(on_init_device);','void register_events(){reshade::register_event<reshade::addon_event::init_device>(on_init_device);','remove b12 observer register')
    text=one(text,'void unregister_events(){reshade::unregister_event<reshade::addon_event::push_descriptors>(observe_host_b12);reshade::unregister_event<reshade::addon_event::present>(present_v34);','void unregister_events(){reshade::unregister_event<reshade::addon_event::present>(present_v34);','remove b12 observer unregister')

    old='const bool calls_ok=install_calls();if(!calls_ok)reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V4.2] retail callsite guard failed; bridge fail-open");const bool t10_ok=install_host_t10_stitch();if(!t10_ok)reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V4.2] build131 t10 stitch hook failed; PRESENT fail-open");else reshade::log::message(reshade::log::level::info,"[DSRRL FULL ENVSPEC V4.2] full t10+b12 input stitch armed");return true;'
    new='const bool calls_ok=install_calls();if(!calls_ok)reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V4.3] retail callsite guard failed; bridge fail-open");const bool providers_ok=resolve_host_owned_providers();if(!providers_ok)reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V4.3] exact build131 provider ABI guard failed; PRESENT fail-open");else reshade::log::message(reshade::log::level::info,"[DSRRL FULL ENVSPEC V4.3] deterministic host t10+b12 providers armed");return true;'
    text=one(text,old,new,'provider init')
    text=one(text,'using namespace dsrrl::runtime_v2::full_envspec_v3;uninstall_host_input_stitch();unpatch_calls();unregister_events();release_owned_dedicated();','using namespace dsrrl::runtime_v2::full_envspec_v3;unpatch_calls();unregister_events();release_owned_dedicated();','provider uninit')

    telpat=r'char stitch\[640\]\{\};std::snprintf\(stitch,sizeof\(stitch\),"\[DSRRL V4\.2 INPUT STITCH\].*?reshade::log::message\(reshade::log::level::info,stitch\);'
    tel='char stitch[640]{};std::snprintf(stitch,sizeof(stitch),"[DSRRL V4.3 OWNED PROVIDERS] guard=%llu/%llu t10=%llu/%llu bind=%llu b12=%llu/%llu bind=%llu specpath=%llu/%llu/%llu",static_cast<unsigned long long>(g_v43_provider_guard_pass.load()),static_cast<unsigned long long>(g_v43_provider_guard_fail.load()),static_cast<unsigned long long>(g_v43_t10_hit.load()),static_cast<unsigned long long>(g_v43_t10_miss.load()),static_cast<unsigned long long>(g_t10_stitch_bind.load()),static_cast<unsigned long long>(g_v43_b12_hit.load()),static_cast<unsigned long long>(g_v43_b12_miss.load()),static_cast<unsigned long long>(g_b12_stitch_bind.load()),static_cast<unsigned long long>(g_v43_spec_path_canonical.load()),static_cast<unsigned long long>(g_v43_spec_path_legacy.load()),static_cast<unsigned long long>(g_v43_spec_path_missing.load()));reshade::log::message(reshade::log::level::info,stitch);'
    text,n=re.subn(telpat,tel,text,count=1,flags=re.S)
    if n!=1: raise SystemExit(f'telemetry replacement count={n}')

    text=text.replace('V4.2 Full Input Stitch EnvSpec','V4.3 Owned Host Providers EnvSpec')
    text=text.replace('[DSRRL FULL ENVSPEC V4.2]','[DSRRL FULL ENVSPEC V4.3]')
    text=text.replace('V4.2 P_Metal EnvSpec island armed with t10+b12 input contract','V4.3 P_Metal EnvSpec island armed with deterministic host providers')
    text=text.replace('Exact build131 + exact P_Metal + shader_index 894/913/932 -> SHA-guarded V4.1 operator island; stitches host-selected PTDE SpecRGB t10 and PTDE donor b12 into the same native PS+t12/t14+s12/s14 transaction; full restore; fail-open elsewhere.','Exact build131 + exact P_Metal + shader_index 894/913/932 -> SHA-guarded V4.1 operator island; actively acquires PTDE SpecRGB t10 through the exact build131 SpecRGB provider and PTDE donor b12 through the exact route345 host provider, then binds PS+t10+b12+t12/t14+s12/s14 in one native transaction with full restore; fail-open elsewhere.')

    required=('k_host_spec_prepare_rva = 0x11350Cu','k_host_spec_bind_rva = 0x113B45u','k_host_b12_provider_rva = 0x77A0u','k_exact_pmetal_route = 345','acquire_owned_host_t10','acquire_owned_host_b12','PSSetShaderResources(10,1,&nullv)','DSRRL V4.3 OWNED PROVIDERS','V4.3 Owned Host Providers EnvSpec')
    for tok in required:
        if tok not in text: raise SystemExit('missing '+tok)
    forbidden=('install_host_t10_stitch()','observe_host_b12(','descriptor_type::constant_buffer||update.count')
    for tok in forbidden:
        if tok in text: raise SystemExit('legacy passive dependency remains: '+tok)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(text,encoding='utf-8')
    print('PASS V4.3 deterministic build131 t10+b12 owned-provider carrier')

if __name__=='__main__': main()
