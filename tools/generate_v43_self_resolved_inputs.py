#!/usr/bin/env python3
from __future__ import annotations
import argparse
from pathlib import Path


def one(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f'{label}: anchor missing')
    return text.replace(old, new, 1)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('--input', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    text = a.input.read_text(encoding='utf-8')

    text = text.replace('V4.2 Full Input Stitch EnvSpec', 'V4.3 Self-Resolved Inputs EnvSpec')
    text = text.replace('[DSRRL FULL ENVSPEC V4.2]', '[DSRRL FULL ENVSPEC V4.3]')

    anchor = 'host_t10_bind_fn g_host_t10_original = nullptr;'
    insert = r'''host_t10_bind_fn g_host_t10_original = nullptr;
constexpr std::uintptr_t k_host_t10_prepare_rva = 0x11350Cu;
constexpr std::uintptr_t k_host_t10_commit_rva = 0x113B45u;
constexpr std::uintptr_t k_host_b12_getter_rva = 0x77A0u;
constexpr std::array<std::uint8_t,k_detour_stolen> k_host_t10_prepare_prolog = {
    0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x55,0x53,0x48,0x81,0xEC};
constexpr std::array<std::uint8_t,k_detour_stolen> k_host_t10_commit_prolog = {
    0x56,0x57,0x48,0x83,0xEC,0x28,0x48,0x89,0xCE,0xE8,0xB5,0xF7,0xFF,0xFF,0x48};
constexpr std::array<std::uint8_t,k_detour_stolen> k_host_b12_getter_prolog = {
    0x48,0x89,0x5C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x55,0x57,0x41,0x54,0x41};
using host_t10_prepare_fn = void (__fastcall *)(ID3D11DeviceContext *);
using host_t10_commit_fn = void (__fastcall *)(ID3D11DeviceContext *);
using host_b12_getter_fn = ID3D11Buffer * (__fastcall *)(ID3D11Device *,std::int32_t);
host_t10_prepare_fn g_host_t10_prepare = nullptr;
host_t10_commit_fn g_host_t10_commit = nullptr;
host_b12_getter_fn g_host_b12_getter = nullptr;
std::atomic<std::uint64_t> g_t10_direct_try{0},g_t10_direct_ok{0},g_b12_direct_try{0},g_b12_direct_ok{0};'''
    text = one(text, anchor, insert, 'host helper declarations')

    anchor = 'bool active_material_is_exact_pmetal() noexcept\n{\n    const auto key=g_active_material;'
    if anchor not in text:
        raise SystemExit('resolver insertion anchor missing')
    resolver = r'''bool resolve_host_t10_direct(ID3D11DeviceContext *ctx) noexcept
{
    if(ctx==nullptr||g_host_t10_prepare==nullptr||g_host_t10_commit==nullptr)return false;
    const auto material=g_active_material;
    if(material==0||!active_material_is_exact_pmetal())return false;
    if(g_host_t10_candidate!=nullptr&&g_host_t10_candidate_material==material)return true;
    ++g_t10_direct_try;
    ID3D11ShaderResourceView *prior=nullptr;
    ctx->PSGetShaderResources(10,1,&prior);
    __try { g_host_t10_prepare(ctx); g_host_t10_commit(ctx); }
    __except(EXCEPTION_EXECUTE_HANDLER) { if(prior){ctx->PSSetShaderResources(10,1,&prior);prior->Release();} return false; }
    ctx->PSSetShaderResources(10,1,&prior);
    if(prior)prior->Release();
    const bool ok=g_host_t10_candidate!=nullptr&&g_host_t10_candidate_material==material;
    if(ok)++g_t10_direct_ok;
    return ok;
}

bool resolve_host_b12_direct(ID3D11DeviceContext *ctx) noexcept
{
    if(ctx==nullptr||g_host_b12_getter==nullptr)return false;
    const auto material=g_active_material;
    if(material==0||!active_material_is_exact_pmetal())return false;
    if(g_host_b12_candidate!=nullptr&&g_host_b12_candidate_material==material)return true;
    ++g_b12_direct_try;
    ID3D11Device *dev=nullptr;ctx->GetDevice(&dev);if(dev==nullptr)return false;
    ID3D11Buffer *donor=nullptr;
    __try { donor=g_host_b12_getter(dev,345); } __except(EXCEPTION_EXECUTE_HANDLER) { donor=nullptr; }
    dev->Release();
    if(donor==nullptr)return false;
    if(g_host_b12_candidate!=nullptr)g_host_b12_candidate->Release();
    g_host_b12_candidate=donor;g_host_b12_candidate_material=material;++g_b12_capture_hit;++g_b12_direct_ok;
    return true;
}

'''
    decl = 'bool active_material_is_exact_pmetal() noexcept;\n\n' + resolver
    text = text.replace(anchor, decl + anchor, 1)

    old = '''bool install_host_t10_stitch()\n{\n    HMODULE host=GetModuleHandleW(L"DSRRL_Material_Response_1.45.addon64");\n    if(host==nullptr)return false;\n    auto *target=reinterpret_cast<std::uint8_t *>(host)+k_host_t10_bind_rva;\n    if(!install_detour(g_host_t10_detour,target,reinterpret_cast<const void *>(&host_t10_bind_hook),k_host_t10_bind_prolog))return false;\n    g_host_t10_original=reinterpret_cast<host_t10_bind_fn>(g_host_t10_detour.trampoline);\n    return g_host_t10_original!=nullptr;\n}'''
    new = '''bool install_host_t10_stitch()\n{\n    HMODULE host=GetModuleHandleW(L"DSRRL_Material_Response_1.45.addon64");\n    if(host==nullptr)return false;\n    auto *base=reinterpret_cast<std::uint8_t *>(host);\n    auto *prepare=base+k_host_t10_prepare_rva;auto *commit=base+k_host_t10_commit_rva;auto *getter=base+k_host_b12_getter_rva;\n    if(std::memcmp(prepare,k_host_t10_prepare_prolog.data(),k_host_t10_prepare_prolog.size())!=0||\n       std::memcmp(commit,k_host_t10_commit_prolog.data(),k_host_t10_commit_prolog.size())!=0||\n       std::memcmp(getter,k_host_b12_getter_prolog.data(),k_host_b12_getter_prolog.size())!=0)return false;\n    g_host_t10_prepare=reinterpret_cast<host_t10_prepare_fn>(prepare);\n    g_host_t10_commit=reinterpret_cast<host_t10_commit_fn>(commit);\n    g_host_b12_getter=reinterpret_cast<host_b12_getter_fn>(getter);\n    auto *target=base+k_host_t10_bind_rva;\n    if(!install_detour(g_host_t10_detour,target,reinterpret_cast<const void *>(&host_t10_bind_hook),k_host_t10_bind_prolog)){g_host_t10_prepare=nullptr;g_host_t10_commit=nullptr;g_host_b12_getter=nullptr;return false;}\n    g_host_t10_original=reinterpret_cast<host_t10_bind_fn>(g_host_t10_detour.trampoline);\n    return g_host_t10_original!=nullptr;\n}'''
    text = one(text, old, new, 'install direct host helpers')

    old = '''void uninstall_host_input_stitch() noexcept\n{\n    clear_host_input_candidates();\n    uninstall_detour(g_host_t10_detour);\n    g_host_t10_original=nullptr;\n}'''
    new = '''void uninstall_host_input_stitch() noexcept\n{\n    clear_host_input_candidates();\n    uninstall_detour(g_host_t10_detour);\n    g_host_t10_original=nullptr;g_host_t10_prepare=nullptr;g_host_t10_commit=nullptr;g_host_b12_getter=nullptr;\n}'''
    text = one(text, old, new, 'uninstall direct host helpers')

    old = '''if(g_host_t10_candidate==nullptr||g_host_t10_candidate_material!=g_active_material){++g_t10_capture_miss;++g_fail_open;return std::nullopt;}\n        if(g_host_b12_candidate==nullptr||g_host_b12_candidate_material!=g_active_material){++g_b12_capture_miss;++g_fail_open;return std::nullopt;}'''
    new = '''if((g_host_t10_candidate==nullptr||g_host_t10_candidate_material!=g_active_material)&&!resolve_host_t10_direct(p.old.ctx)){++g_t10_capture_miss;++g_fail_open;return std::nullopt;}\n        if((g_host_b12_candidate==nullptr||g_host_b12_candidate_material!=g_active_material)&&!resolve_host_b12_direct(p.old.ctx)){++g_b12_capture_miss;++g_fail_open;return std::nullopt;}'''
    text = one(text, old, new, 'direct input resolution gate')

    old = 'reshade::log::message(reshade::log::level::info,stitch);'
    new = '''reshade::log::message(reshade::log::level::info,stitch);char direct[320]{};std::snprintf(direct,sizeof(direct),"[DSRRL V4.3 SELF RESOLVE] t10 try=%llu ok=%llu | b12 try=%llu ok=%llu",static_cast<unsigned long long>(g_t10_direct_try.load()),static_cast<unsigned long long>(g_t10_direct_ok.load()),static_cast<unsigned long long>(g_b12_direct_try.load()),static_cast<unsigned long long>(g_b12_direct_ok.load()));reshade::log::message(reshade::log::level::info,direct);'''
    text = one(text, old, new, 'direct telemetry')

    text = text.replace('[DSRRL V4.2 INPUT STITCH]', '[DSRRL V4.3 INPUT STITCH]')
    text = text.replace('full t10+b12 input stitch armed', 'self-resolved PTDE t10+b12 input stitch armed')

    required = (
        'V4.3 Self-Resolved Inputs EnvSpec',
        '[DSRRL V4.3 SELF RESOLVE]',
        'k_host_t10_prepare_rva = 0x11350Cu',
        'k_host_t10_commit_rva = 0x113B45u',
        'k_host_b12_getter_rva = 0x77A0u',
        'resolve_host_t10_direct(p.old.ctx)',
        'resolve_host_b12_direct(p.old.ctx)',
        'PSSetShaderResources(10,1,&v)',
        'PSSetConstantBuffers(12,1,&v)',
    )
    for token in required:
        if token not in text:
            raise SystemExit('missing output token: '+token)

    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(text,encoding='utf-8')
    print('PASS V4.3 self-resolved host PTDE input carrier')


if __name__ == '__main__':
    main()
