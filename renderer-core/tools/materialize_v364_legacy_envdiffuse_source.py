#!/usr/bin/env python3
"""V364 stage A: exact retail legacy t11 producer -> independent PTDE donor,
without copying any t12/t14 selector, shader modification or nearest probe.
Requires V362+V363 materialized C++ sources.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"src/runtime/envspec_resource_runtime.cpp"
s=p.read_text("utf8")
def once(a,b,tag):
    global s
    n=s.count(a)
    if n!=1:raise SystemExit("V364 A "+tag+" source drift count="+str(n))
    s=s.replace(a,b,1)

once('#include "dsrrl/runtime/gi_envdif_name_witness.hpp"',
     '#include "dsrrl/runtime/gi_envdif_name_witness.hpp"\n'
     '#include "dsrrl/operators/env_spec/legacy_envdiffuse_m18_exact.hpp"',
     "source carrier API")

once('std::vector<std::uint8_t> g_envdiffuse_pack;',
'''std::vector<std::uint8_t> g_envdiffuse_pack;
// Exact original PTDE legacy, keyed ONLY by separately verified DSR t11.
std::array<std::vector<std::uint8_t>,2u> g_v364_legacy_rgbas{};
std::atomic<std::uint32_t> g_v364_legacy_prepare_logged{0u};''',
     "bounded asset store")

once('bool is_native_envspec_desc(',
'''bool load_v364_legacy_rgba(
    const operators::env_spec::exact_ptde_legacy_envdiffuse &entry,
    std::vector<std::uint8_t> &dst) noexcept
{
    dst.clear();
    try {
        const auto root=process_dir();
        if(root.empty())return false;
        const auto path=root/std::filesystem::path(entry.relative_path);
        std::error_code ec{};
        const auto n=std::filesystem::file_size(path,ec);
        if(ec || n!=entry.bytes)return false;
        std::ifstream file(path,std::ios::binary);
        if(!file)return false;
        std::vector<std::uint8_t> source(static_cast<std::size_t>(n));
        if(!file.read(reinterpret_cast<char *>(source.data()),
            static_cast<std::streamsize>(source.size())))return false;
        if(hashing::sha256(source.data(),source.size())!=entry.ptde_asset_sha)
            return false;
        dst=std::move(source);
        return true;
    } catch(...) {
        dst.clear();
        return false;
    }
}

// No asset name can authorize on its own. Name and creation SHA must
// independently refer to the SAME live native t11 SRV backing.
bool v364_exact_legacy_selector_for_view(
    ID3D11ShaderResourceView *view,std::uint16_t &selector) noexcept
{
    selector=0u;
    if(view==nullptr)return false;
    ID3D11Resource *resource=nullptr;
    ID3D11Device *device=nullptr;
    view->GetResource(&resource);
    view->GetDevice(&device);
    bool valid=false;
    if(resource && device) {
        const auto key=reinterpret_cast<std::uint64_t>(resource);
        std::lock_guard<std::mutex> lock(g_mutex);
        if(g_device &&
           reinterpret_cast<std::uintptr_t>(g_device->get_native())==
               reinterpret_cast<std::uintptr_t>(device)) {
            const auto named=g_gi_name_by_resource.find(key);
            const auto sha=g_diffuse_creation_sha.find(key);
            if(named!=g_gi_name_by_resource.end() &&
               sha!=g_diffuse_creation_sha.end() &&
               !named->second.ambiguous) {
                const auto *donor=operators::env_spec::v364_exact_legacy_diffuse(
                    named->second.logical_name.data(),sha->second);
                if(donor) {
                    const unsigned index=donor->selector==500u?0u:1u;
                    if(!g_v364_legacy_rgbas[index].empty()) {
                        selector=donor->selector;
                        valid=true;
                    }
                }
            }
        }
    }
    if(resource)resource->Release();
    if(device)device->Release();
    return valid;
}

bool is_native_envspec_desc(''',"producer/name/SHA independent resolver")

once('    const bool envdiffuse_pack_ready =\n        load_envdiffuse_pack(\n            envdiffuse_pack);',
'''    const bool envdiffuse_pack_ready =
        load_envdiffuse_pack(
            envdiffuse_pack);
    std::array<std::vector<std::uint8_t>,2u> v364_assets{};
    for(std::size_t i=0u;
        i<operators::env_spec::k_v364_m18_legacy_envdiffuse.size();++i)
        load_v364_legacy_rgba(
            operators::env_spec::k_v364_m18_legacy_envdiffuse[i],
            v364_assets[i]);''',"asset read at init_device")

once('            g_envdiffuse_pack_ready =\n                envdiffuse_pack_ready;',
'''            g_envdiffuse_pack_ready =
                envdiffuse_pack_ready;
            g_v364_legacy_rgbas=std::move(v364_assets);''',"atomic adoption")

once('        g_envdiffuse_pack.clear();',
     '        g_envdiffuse_pack.clear();\n        g_v364_legacy_rgbas={};',
     "release assets on device destroy")

p.write_text(s,"utf8")
print("V364 EXACT LEGACY t11 NAMES+SHA+PTDE ASSETS LOADED; no t12 alias")
