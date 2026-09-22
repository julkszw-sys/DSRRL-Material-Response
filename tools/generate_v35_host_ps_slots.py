#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
from pathlib import Path

HOST_SLOT_BLOCK = r'''
inline constexpr std::array<std::uintptr_t,3> k_build131_ps_slot_rvas = {
    0x112308u, 0x112328u, 0x112348u
};

std::uintptr_t read_build131_ps_slot(std::uintptr_t address) noexcept
{
    __try {
        return *reinterpret_cast<const std::uintptr_t *>(address);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
'''.strip()

NATIVE_RECEIVER = r'''
std::optional<receiver_rec> native_receiver(reshade::api::command_list *cmd)
{
    if(cmd==nullptr||cmd->get_native()==0)return std::nullopt;
    auto *ctx=reinterpret_cast<ID3D11DeviceContext *>(static_cast<std::uintptr_t>(cmd->get_native()));
    ID3D11PixelShader *ps=nullptr;
    ctx->PSGetShader(&ps,nullptr,nullptr);
    if(ps==nullptr){++g_ps_unknown;return std::nullopt;}

    const auto actual=reinterpret_cast<std::uintptr_t>(ps);

    // Build131 creates DXBC72/73/74 before this companion is loaded and stores
    // their live D3D11 pixel-shader handles in three module-local runtime slots.
    // Read those exact selector-owned slots directly so PRESENT identity is not
    // dependent on creation-time ReShade init_pipeline observation order.
    if(HMODULE host=GetModuleHandleW(L"DSRRL_Material_Response_1.45.addon64");host!=nullptr){
        const auto base=reinterpret_cast<std::uintptr_t>(host);
        std::uint64_t ready=0;
        for(std::size_t i=0;i<k_build131_ps_slot_rvas.size();++i){
            const std::uintptr_t candidate=read_build131_ps_slot(base+k_build131_ps_slot_rvas[i]);
            if(candidate!=0)++ready;
            if(candidate!=0&&candidate==actual){
                g_ps_dedicated_created.store(ready,std::memory_order_relaxed);
                ++g_ps_dedicated_match;
                ps->Release();
                return receiver_rec{receiver_kind::dedicated,static_cast<std::uint32_t>(20072u+i)};
            }
        }
        g_ps_dedicated_created.store(ready,std::memory_order_relaxed);
    }

    // Stock no-PointLight receivers are still observed after companion load, so
    // creation-time registry remains valid for EXPLICIT_NONE only.
    const auto h=static_cast<std::uint64_t>(actual);
    ps->Release();
    std::lock_guard lock(g_mutex);
    const auto it=g_ps.find(h);
    if(it==g_ps.end()){++g_ps_unknown;return std::nullopt;}
    if(it->second.kind==receiver_kind::dedicated)++g_ps_dedicated_match;
    else ++g_ps_stock_match;
    return it->second;
}
'''.strip()


def main() -> None:
    ap=argparse.ArgumentParser()
    ap.add_argument('--input',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args()

    text=args.input.read_text(encoding='utf-8')

    anchor='constexpr char k_build131_sha[] = "db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966";'
    if anchor not in text:
        raise SystemExit('build131 SHA anchor missing')
    text=text.replace(anchor,anchor+'\n'+HOST_SLOT_BLOCK,1)

    pat=re.compile(
        r'std::optional<receiver_rec> native_receiver\(reshade::api::command_list \*cmd\)\n\{.*?\n\}\n\n(?=std::optional<reshade::api::resource_view> raw_cube)',
        re.S,
    )
    text,n=pat.subn(NATIVE_RECEIVER+'\n\n',text,count=1)
    if n!=1:
        raise SystemExit(f'native_receiver replacement count={n}')

    text=text.replace('V3.4 Native PS Gate','V3.5 Host PS Slots')
    text=text.replace('[DSRRL FULL ENVSPEC V3.4]','[DSRRL FULL ENVSPEC V3.5]')
    text=text.replace('V3.4 native PS gate armed','V3.5 direct build131 PS-slot gate armed')

    required=(
        '0x112308u, 0x112328u, 0x112348u',
        'read_build131_ps_slot',
        'GetModuleHandleW(L"DSRRL_Material_Response_1.45.addon64")',
        'PSGetShader',
        'candidate==actual',
        'r8g8b8a8_unorm',
        'V3.5 Host PS Slots',
    )
    for token in required:
        if token not in text:
            raise SystemExit(f'missing generated token: {token}')

    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(text,encoding='utf-8')
    print('PASS V3.5 direct host-slot gate')


if __name__=='__main__':
    main()
