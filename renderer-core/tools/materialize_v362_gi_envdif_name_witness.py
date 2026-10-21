#!/usr/bin/env python3
"""V362: exact TLS GI EnvDif name -> native D3D11 resource -> rejected t11.
Read-only witness. Does not authorize, remap or replace any GPU resource.
Execute after all V230/V359 materializers, before native compilation.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/"src/runtime/envspec_resource_runtime.cpp"
s=p.read_text("utf-8")
def once(a,b):
    global s
    if s.count(a)!=1:raise RuntimeError("V362 source drift "+a[:90]+" count="+str(s.count(a)))
    s=s.replace(a,b,1)

once('#include "dsrrl/runtime/texture_identity_transport.hpp"',
     '#include "dsrrl/runtime/texture_identity_transport.hpp"\n#include "dsrrl/runtime/gi_envdif_name_witness.hpp"') if '#include "dsrrl/runtime/texture_identity_transport.hpp"' in s else once(
     '#include "dsrrl/runtime/v214_resource_name_provenance.hpp"',
     '#include "dsrrl/runtime/v214_resource_name_provenance.hpp"\n#include "dsrrl/runtime/gi_envdif_name_witness.hpp"')

once('    g_diffuse_creation_sha;',
'''    g_diffuse_creation_sha;
// Read-only GI source name. No effect on certified SHA allowlist.
std::unordered_map<std::uint64_t,gi_envdif_name_witness::identity>
    g_gi_name_by_resource;
void observe_gi_name_locked(std::uint64_t id,
    gi_envdif_name_witness::identity incoming) noexcept
{
    if(!id || incoming.name[0]==0) return;
    try {
        auto it=g_gi_name_by_resource.find(id);
        if(it==g_gi_name_by_resource.end()) {
            if(g_gi_name_by_resource.size()<4096u)
                g_gi_name_by_resource.emplace(id,incoming);
        } else if(it->second.name!=incoming.name) {
            it->second.conflicting=true;
        }
    } catch(...) {}
}
''')

once('        g_diffuse_creation_sha.clear();',
     '        g_diffuse_creation_sha.clear();\n        g_gi_name_by_resource.clear();')

once('    if (diffuse) ++g_diffuse_create_candidates;',
'''    if (diffuse) ++g_diffuse_create_candidates;
    if (diffuse) {
        gi_envdif_name_witness::identity name{};
        if(gi_envdif_name_witness::capture(name)) {
            name.stage=1u;
            std::lock_guard<std::mutex> lock(g_mutex);
            if(device_ptr==g_device)
                observe_gi_name_locked(resource_handle.handle,name);
        }
    }''')

once('    g_diffuse_creation_sha.erase(resource_handle.handle);',
     '    g_diffuse_creation_sha.erase(resource_handle.handle);\n    g_gi_name_by_resource.erase(resource_handle.handle);')

once('''    std::lock_guard<std::mutex> lock(
        g_mutex);

    if (device_ptr != g_device)
        return;

    const auto native =
        g_native_resources.find(
            resource_handle.handle);''',
'''    gi_envdif_name_witness::identity gi_name{};
    bool same_backing=false;
    if (gi_envdif_name_witness::capture(gi_name)) {
        auto *srv=reinterpret_cast<ID3D11ShaderResourceView *>(view.handle);
        ID3D11Resource *native_resource=nullptr;
        srv->GetResource(&native_resource);
        same_backing = native_resource!=nullptr &&
            reinterpret_cast<std::uintptr_t>(native_resource)==
            static_cast<std::uintptr_t>(resource_handle.handle);
        if(native_resource!=nullptr)native_resource->Release();
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (device_ptr != g_device) return;
    if(same_backing &&
        g_diffuse_creation_sha.find(resource_handle.handle)!=
        g_diffuse_creation_sha.end()) {
        gi_name.stage=2u;
        observe_gi_name_locked(resource_handle.handle,gi_name);
    }

    const auto native =
        g_native_resources.find(
            resource_handle.handle);''')

once('    bool creation_sha_joined = false;',
'''    bool creation_sha_joined = false;
    gi_envdif_name_witness::identity gi_name{};
    bool gi_name_joined=false;''')

once('''            const auto already = g_diffuse_by_view.find(key);''',
'''            auto name_it=g_gi_name_by_resource.find(resource_key);
            if(name_it==g_gi_name_by_resource.end())
                name_it=g_gi_name_by_resource.find(texture_key);
            if(name_it!=g_gi_name_by_resource.end()) {
                gi_name=name_it->second;
                gi_name_joined=true;
            }
            const auto already = g_diffuse_by_view.find(key);''')

once('''        reshade::log::message(reshade::log::level::warning,witness);
        if (native_device != nullptr) native_device->Release();''',
'''        reshade::log::message(reshade::log::level::warning,witness);
        char line[360]{};
        std::snprintf(line,sizeof(line),
            "[DSRRL V362 T11 GI NAME JOIN]"
            " srv=%016llx backing=%016llx sha_join=%u"
            " gi_name=%.96s gi_stage=%u gi_status=%s"
            " authorization=UNCHANGED action=STOCK_DSR pixels=OPEN",
            static_cast<unsigned long long>(key),
            static_cast<unsigned long long>(resource_key),
            creation_sha_joined?1u:0u,
            gi_name_joined?gi_name.name.data():"UNAVAILABLE",
            gi_name_joined?gi_name.stage:0u,
            !gi_name_joined?"NO_EXACT_TLS_GI_NAME":
                gi_name.conflicting?"AMBIGUOUS_FAIL_OPEN":
                    "EXACT_TLS_NATIVE_RESOURCE");
        reshade::log::message(reshade::log::level::warning,line);
        if (native_device != nullptr) native_device->Release();''')

p.write_text(s,"utf-8")
print("V362_GI_EXACT_TLS_SRV_WITNESS_SOURCE_PASS")
