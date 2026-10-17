#!/usr/bin/env python3
"""V357 negative-path only: provenance for unknown stock t1 and BC6H t12.
Construction after V351-V356 materializers, before CMake; diagnostic-only.
No guessed asset/probe/owner, changes to acceptance gates or live hot draws.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]

def apply(path,old,new,label):
    p=root/path
    s=p.read_text("utf8")
    n=s.count(old)
    if n!=1:raise SystemExit(f"V357 anchor {label} mismatch: {n}")
    p.write_text(s.replace(old,new,1),"utf8")

p="src/runtime/pmetal_envspec_draw_runtime.cpp"
apply(p,"#include <cmath>","#include <algorithm>\n#include <cmath>","standard min header")

apply(p,
'#include "dsrrl/runtime/pixel_srv_shadow.hpp"',
'#include "dsrrl/runtime/pixel_srv_shadow.hpp"\n'
'#include "dsrrl/runtime/v214_resource_name_provenance.hpp"',
"add source provenance header")

anchor='std::atomic<std::uint32_t> g_value_cut_log_mask{0u};'
helper=r"""
#if defined(DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS)
// Called ONLY in a previously rate-limited rejected receiver/slot bucket.
// Native Get* methods are read-only. Never infer PTDE identity from format,
// geometry, MTD or a similar resource. The producer source is taken only
// from the V214 original retail texture-name/resource registry.
void v357_trace_rejected_srv(
    ID3D11DeviceContext *context,
    unsigned binding,
    unsigned receiver,
    unsigned route,
    unsigned material_slot) noexcept
{
    if (context==nullptr) return;
    ID3D11ShaderResourceView *srv=nullptr;
    context->PSGetShaderResources(binding,1u,&srv);
    if (srv==nullptr) {
        char line[224]{};
        std::snprintf(line,sizeof(line),
          "[DSRRL V357 RESOURCE SOURCE] t=%u rx=%u route=%u slot=%u"
          " srv=UNBOUND source=NO_GPU_VIEW pixel=OPEN",
          binding,receiver,route,material_slot);
        reshade::log::message(reshade::log::level::info,line);
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC vd{};
    srv->GetDesc(&vd);
    ID3D11Resource *resource=nullptr;
    srv->GetResource(&resource);
    ID3D11Device *device=nullptr;
    srv->GetDevice(&device);
    ID3D11Texture2D *texture=nullptr;
    D3D11_TEXTURE2D_DESC td{};
    const bool texture2d =
        resource!=nullptr &&
        SUCCEEDED(resource->QueryInterface(
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void **>(&texture))) &&
        texture!=nullptr;
    if(texture2d) texture->GetDesc(&td);

    std::array<wchar_t,257> producer_name{};
    std::size_t producer_length=0u;
    const auto producer_status=
        v214_resource_provenance::instance().snapshot(
            reinterpret_cast<std::uintptr_t>(device),
            reinterpret_cast<std::uintptr_t>(resource),
            producer_name,producer_length);
    // Printable, bounded raw original retail name, never invented FNV/hash.
    char original_name[69]{};
    if(producer_status==v214_resource_provenance::status::exact) {
        const auto n=std::min(producer_length,
                             sizeof(original_name)-1u);
        for(std::size_t i=0u;i<n;++i) {
            const auto ch=producer_name[i];
            original_name[i]=(ch>=33 && ch<=126)
                ?static_cast<char>(ch):'?';
        }
    }
    const char *origin=producer_status==
        v214_resource_provenance::status::exact
        ? "EXACT_RETAIL_SOURCE"
        : producer_status==v214_resource_provenance::status::ambiguous
            ? "AMBIGUOUS_SOURCE" : "NO_PRODUCER_WITNESS";

    char line[960]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V357 RESOURCE SOURCE] t=%u rx=%u route=%u slot=%u"
        " srv=%016llx backing=%016llx device=%016llx"
        " srv_dim=%u srv_fmt=%u mip0=%u mip_count=%u"
        " texture2d=%u w=%u h=%u mips=%u array=%u tex_fmt=%u"
        " bind=0x%x usage=%u cpu=0x%x misc=0x%x"
        " producer=%s source_name=%.68s"
        " backing_identity=OBSERVED_NOT_ASSIGNED pixel=OPEN",
        binding,receiver,route,material_slot,
        static_cast<unsigned long long>(
            reinterpret_cast<std::uintptr_t>(srv)),
        static_cast<unsigned long long>(
            reinterpret_cast<std::uintptr_t>(resource)),
        static_cast<unsigned long long>(
            reinterpret_cast<std::uintptr_t>(device)),
        static_cast<unsigned>(vd.ViewDimension),
        static_cast<unsigned>(vd.Format),
        vd.Texture2D.MostDetailedMip,vd.Texture2D.MipLevels,
        texture2d?1u:0u,
        td.Width,td.Height,td.MipLevels,td.ArraySize,
        static_cast<unsigned>(td.Format),td.BindFlags,
        static_cast<unsigned>(td.Usage),
        td.CPUAccessFlags,td.MiscFlags,
        origin,original_name);
    reshade::log::message(reshade::log::level::info,line);
    if(texture!=nullptr) texture->Release();
    if(device!=nullptr) device->Release();
    if(resource!=nullptr) resource->Release();
    srv->Release();
}
#endif
"""
apply(p,anchor,anchor+helper,"V357 negative trace helper")

needle='''            reshade::log::message(reshade::log::level::info,line);
        }
#endif
        if (shader != nullptr)
            shader->Release();'''
repl='''            reshade::log::message(reshade::log::level::info,line);
#if defined(DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS)
            v357_trace_rejected_srv(
                context,12u,decision.receiver_id,
                decision.route_index,
                static_cast<unsigned>(material.material_slot));
            if(probe_b_required)
                v357_trace_rejected_srv(
                    context,14u,decision.receiver_id,
                    decision.route_index,
                    static_cast<unsigned>(material.material_slot));
#endif
        }
#endif
        if (shader != nullptr)
            shader->Release();'''
apply(p,needle,repl,"V354 bounded t12 and t14 reject")

needle='''            reshade::log::message(reshade::log::level::info,line);
        }
#endif
        log_prepare_stage_once('''
# Need identify actual context following spec reject. Print anchor check instead of guessing.
s=(root/p).read_text("utf8")
idx=s.index("[DSRRL V354 SPEC REJECT]")
tail=s[idx:idx+2050]
if "reshade::log::message(reshade::log::level::info,line);" not in tail:
    raise SystemExit("V357 spec rejection logging absent")
log='reshade::log::message(reshade::log::level::info,line);'
pos=s.index(log,idx)
# locate exact log following V354 SPEC ONLY
s=s[:pos]+s[pos:].replace(log,log+'''
#if defined(DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS)
            v357_trace_rejected_srv(
                context,1u,decision.receiver_id,
                decision.route_index,
                static_cast<unsigned>(material.material_slot));
#endif''',1)
(root/p).write_text(s,"utf8")

# EnvSpec: capture SHA on producer creation of otherwise unrecognized
# EXACT 256px BC6H resources. No readback and no per-frame hashing.
p="src/runtime/envspec_resource_runtime.cpp"
apply(p,"#include <cstdint>","#include <cstdint>\n#include <cstdio>","stdio producer telemetry")
marker='std::atomic<std::uint64_t> g_native_hash_miss{0};'
apply(p,marker,marker+'''
#if defined(DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS)
std::atomic<std::uint32_t> g_v357_unknown_bc6h_census{0u};
std::atomic<std::uint32_t> g_v357_missing_bc6h_init{0u};
#endif''',"counter")
marker='''        ++g_native_hash_miss;
        return;
    }

    const auto digest =
        hashing::sha256('''
repl='''        ++g_native_hash_miss;
#if defined(DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS)
        const auto n=g_v357_missing_bc6h_init.fetch_add(
            1u,std::memory_order_relaxed);
        if(n<16u) {
            char line[320]{};
            std::snprintf(line,sizeof(line),
                "[DSRRL V357 BC6H PRODUCER] stage=initial_data_not_extractable"
                " resource=%016llx candidate_desc=YES"
                " exact_sha=UNAVAILABLE fail_open=1",
                static_cast<unsigned long long>(resource_handle.handle));
            reshade::log::message(reshade::log::level::info,line);
        }
#endif
        return;
    }

    const auto digest =
        hashing::sha256('''
apply(p,marker,repl,"producer extraction unknown")
marker='''    if (!probe.has_value()) {
        ++g_native_hash_miss;
        return;
    }

    {
        std::lock_guard<std::mutex> lock('''
repl='''    if (!probe.has_value()) {
        ++g_native_hash_miss;
#if defined(DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS)
        const auto n=g_v357_unknown_bc6h_census.fetch_add(
            1u,std::memory_order_relaxed);
        if(n<96u) {
            char digest_hex[65]{};
            constexpr char alphabet[]="0123456789abcdef";
            for(std::size_t i=0u;i<digest.size();++i) {
                digest_hex[i*2u]=alphabet[digest[i]>>4u];
                digest_hex[i*2u+1u]=alphabet[digest[i]&15u];
            }
            char line[320]{};
            std::snprintf(line,sizeof(line),
                "[DSRRL V357 BC6H PRODUCER]"
                " stage=sha_not_in_certified_atlas"
                " resource=%016llx sha256=%s"
                " tight_bytes=%u exact_probe=NONE pixel=OPEN",
                static_cast<unsigned long long>(resource_handle.handle),
                digest_hex,static_cast<unsigned>(tight.size()));
            reshade::log::message(reshade::log::level::info,line);
        }
#endif
        return;
    }

    {
        std::lock_guard<std::mutex> lock('''
apply(p,marker,repl,"producer SHA not in atlas")
print("V357_SOURCE_PROVENANCE_NEGATIVE_PATH_INSTRUMENTATION_APPLIED")
