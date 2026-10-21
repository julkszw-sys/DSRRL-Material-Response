#!/usr/bin/env python3
"""V363 original retail legacy EnvSpc: exact name + native creation SHA.
Diagnostic only, after V362 and V358 source materializers.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"src/runtime/envspec_resource_runtime.cpp"
s=p.read_text("utf8")
def one(a,b):
    global s
    if s.count(a)!=1:raise SystemExit("V363 source anchor drift: "+a[:60]+" count="+str(s.count(a)))
    s=s.replace(a,b,1)
one('#include "dsrrl/runtime/gi_envdif_name_witness.hpp"',
    '#include "dsrrl/runtime/gi_envdif_name_witness.hpp"\n#include "dsrrl/runtime/gi_envspec_name_witness.hpp"')
one('    const auto digest = hashing::sha256(tight.data(),tight.size());',
'''    const auto digest = hashing::sha256(tight.data(),tight.size());
    if (spec) {
        gi_envspec_name_witness::identity name{};
        if (gi_envspec_name_witness::capture(name)) {
            // Digest is from the real native D3D11 creation BC6H payload,
            // never from a guess at sibling EnvDiffuse or nearby t12.
            char hex[65]{};
            constexpr char digits[]="0123456789abcdef";
            for(std::size_t i=0;i<digest.size();++i) {
                hex[2*i]=digits[digest[i]>>4u];
                hex[2*i+1]=digits[digest[i]&15u];
            }
            char line[350]{};
            std::snprintf(line,sizeof(line),
                "[DSRRL V363 LEGACY SPEC CREATE]"
                " backing=%016llx name=%.96s"
                " sha256=%s format=BC6H256_8mips"
                " scope=EXACT_NATIVE_CREATION"
                " authorization=UNCHANGED pixel=OPEN",
                static_cast<unsigned long long>(resource_handle.handle),
                name.name.data(),hex);
            reshade::log::message(reshade::log::level::warning,line);
        }
    }''')
# The V359/V362 callback has a single mutex; insert read-only same-resource
# view witness BEFORE that mutex, leaving all existing guards and paths intact.
front,tail=s.split("void on_init_resource_view(",1)
body,after=tail.split("void on_destroy_resource_view(",1)
needle='''    std::lock_guard<std::mutex> lock(
        g_mutex);'''
if body.count(needle)!=1:raise SystemExit("V363 view mutex drift")
body=body.replace(needle,'''    gi_envspec_name_witness::identity legacy{};
    if (device_ptr->get_api()==device_api::d3d11 &&
        gi_envspec_name_witness::capture(legacy)) {
        auto *srv=reinterpret_cast<ID3D11ShaderResourceView *>(
            static_cast<std::uintptr_t>(view.handle));
        ID3D11Resource *backing=nullptr;
        ID3D11Device *native_device=nullptr;
        srv->GetResource(&backing);
        srv->GetDevice(&native_device);
        const bool exact=backing!=nullptr && native_device!=nullptr &&
            reinterpret_cast<std::uintptr_t>(backing)==
                static_cast<std::uintptr_t>(resource_handle.handle) &&
            reinterpret_cast<std::uintptr_t>(native_device)==
                static_cast<std::uintptr_t>(device_ptr->get_native());
        if(backing!=nullptr)backing->Release();
        if(native_device!=nullptr)native_device->Release();
        if(exact) {
            char line[220]{};
            std::snprintf(line,sizeof(line),
                "[DSRRL V363 LEGACY SPEC VIEW]"
                " backing=%016llx srv=%016llx name=%.96s"
                " scope=EXACT_D3D11_COM_RESOURCE"
                " authorization=UNCHANGED pixel=OPEN",
                static_cast<unsigned long long>(resource_handle.handle),
                static_cast<unsigned long long>(view.handle),
                legacy.name.data());
            reshade::log::message(reshade::log::level::warning,line);
        }
    }
'''+needle,1)
s=front+"void on_init_resource_view("+body+"void on_destroy_resource_view("+after
p.write_text(s,"utf8")
print("V363 LEGACY SPEC SOURCE CAPTURE PASS; NO PROBE/SHADER CHANGE")
