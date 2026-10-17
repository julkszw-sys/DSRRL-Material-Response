#!/usr/bin/env python3
"""V359: exact retail GI source-name recovery, guarded by independent SHA/name
attestation of at least two distinct probes on the SAME map.
Authoritative source: DSR GI packed binder 342 rows and R15 342 probe names.
No guesses from ordering at runtime, MTD, shader family or nearby resource.
"""
from pathlib import Path
import hashlib,base64,zlib
root=Path(__file__).resolve().parents[1]
packed="eNp92TFuHDEMheE+pxlSIqU5QGCkzgG2SJ0gVc6fBFg3Xn0GXP0YaPg4fNSz/fbt8fXXn++/fzx+xvW4/v9c+bi+vB34BF/g95nnBY73jjrzwvOFOmuDo86FOhfOX4c64xHXdYEHeIIP8Amuehb4Br/PPFB/4L2B8xP9SZyf6E/2mQ+cM3DOQD+Hzkd/JnRNvHfi/InvNdHPiXoK9RTmqvAdC/U0zm/o2uA3+nOjzht13uc+BPwY8GPAdwHf/RMMjnoC9cBfEQmOOgN1xgLf4Kgf/g34N+DfyEOd+f8eucADPMEHuN5b4A2+wDf4feYBXQFdgToDdQbqDNSTH3k87/cLPMATfIBP8AJv8AW+waE3oDegN6A3oDegN6A3oDegN6A3oDehN6E3oTehN6E3oTehN6E3oVfzPKB3QO+A3gG9A3oH9A7oHdA7oHdA74TeCb0vuSWxBxJ7ILEHEnsgsQcSfk98r8T3SnyvxHfJz/swUefUOaj/Je89eaPPjXN6n/lCf9ZBV+A7Br5LnPbVkw/wOcAbfJ954/nG8+s68xv13NB7Q9eNeu515HFd4AE+wCd4gTc46ny57wb8PuD3Ab8P+H3g3h/YAwNz+84TfIBP8EM9ifcm3pt47zGXTuzbif5P9H+i/xP9n0+9F3iAJ/gA13sLvME/zm2hP4X+FPpT6E9hPgvzWcilhVxayKWFXFrIpYVcWsilhVxayKWFXFrYP4X9U9g/hf1T2D+F/VPYP+X9E/B1wL8B/wb8G/BvwL8B/wb8G/BvwL8B/wb8G/BvwL8B/wb8G/BvwL8B3wV8F/BdwEcBX8Qn8//ye1kjDzTuu8Z917jvGvdd475r3HeNe6pxTzXuqcb+b+z/xv5v7P/G/m/s/8b+b+z/xj5v7PPGPm/s88Y+b+zzxj5v7OHG3weevPB86XnoKugq6CrU//L74IJfFvyy4IuF/LYwzwv5aiEXLeSfhflfmP+FuV2Y24V8sjC3C3O7MLcLOWFh3hb+rrIwbwv/t33n6P/G87f4R70bc7UxVxtztbFvN/LPk798r417ZOMeeef/vtdfE+w/QA=="
raw=zlib.decompress(base64.b64decode(packed))
assert hashlib.sha256(raw).hexdigest()=="3c92de7e08b5bd4291ec683dcf5d36ff5011a4fea7c60c38dacdfd179145325a"
names=raw.decode("ascii").splitlines()
assert len(names)==342 and len(set(names))==342
assert all(x.startswith("GI_EnvSpc_m") and x.endswith("_0") and len(x)<65 for x in names)
h=root/"include/dsrrl/operators/env_spec/generated_gi_name_binder_v359.hpp"
h.parent.mkdir(parents=True,exist_ok=True)
header=['#pragma once','// V359 exact retail GI binder 342 names. Source CSV SHA256:',
'// d10761ddf4f3de0092db91cd7e656e2bb30eaba0c9f8b1cb62f848b45d98a9c9',
'// Independently checked identical ordered names: R15_HDR_CUBEMAP_ALL_342.csv',
'#include <array>','#include <cstdint>',
'namespace dsrrl::operators::env_spec::generated {',
'struct v359_name { const wchar_t *name; std::uint16_t ordinal; std::uint8_t map_id; };',
'inline constexpr std::array<v359_name,342u> k_v359_names = {{']
for i,name in enumerate(names):
    header.append(f'    {{L"{name}", {i}u, {int(name[11:13])}u}},')
header+=['}};','}']
h.write_text("\n".join(header)+"\n","utf8")

def replace(path,old,new,tag):
    p=root/path;s=p.read_text("utf8")
    if s.count(old)!=1:raise RuntimeError(f"V359 anchor {tag}: {s.count(old)}")
    p.write_text(s.replace(old,new,1),"utf8")

p="src/runtime/envspec_resource_runtime.cpp"
# CMake retains original source headers; this is exact resource-name provenance.
replace(p,'#include "dsrrl/operators/env_spec/legacy_resource_bridge.hpp"',
'#include "dsrrl/operators/env_spec/legacy_resource_bridge.hpp"\n#include "dsrrl/operators/env_spec/generated_gi_name_binder_v359.hpp"\n#include "dsrrl/runtime/texture_identity_transport.hpp"',"include")
replace(p,"    bool certified = false;\n    std::uint16_t probe = 0u;\n};\n// A resource pointer",
'''    bool certified = false;
    std::uint16_t probe = 0u;
#if defined(DSRRL_V359_ATTESTED_GI_NAME_ALIAS)
    bool source_name_exact = false;
    std::uint16_t source_ordinal = 0u;
    std::uint8_t source_map = 0u;
#endif
};
// A resource pointer''',"extended lifetime record")

# This helper is defined in the same namespace as the native registry, and
# only the original retail-loader hook is allowed to supply its source name.
helper=r'''
#if defined(DSRRL_V359_ATTESTED_GI_NAME_ALIAS)
std::array<std::array<std::uint64_t,6u>,20u> g_v359_seen_ordinal{};
std::array<std::uint16_t,20u> g_v359_seen_count{};
std::array<bool,20u> g_v359_map_conflict{};
std::atomic<std::uint64_t> g_v359_recovered{0u};

bool v359_match_original_gi_name(
    const wchar_t *name,std::size_t length,
    std::uint16_t &ordinal,std::uint8_t &map_id) noexcept
{
    if(name==nullptr || length<15u || length>64u) return false;
    for(std::size_t i=0;i<length;++i)
        if(name[i]<0x21 || name[i]>0x7e) return false;
    for(const auto &r:operators::env_spec::generated::k_v359_names) {
        std::size_t i=0u;
        while(i<length && r.name[i]!=L'\0' && r.name[i]==name[i])
            ++i;
        if(i==length && r.name[i]==L'\0') {
            ordinal=r.ordinal;
            map_id=r.map_id;
            return map_id<20u;
        }
    }
    return false;
}

bool v359_verified_native_name_probe_locked(
    std::uint64_t backing,std::uint16_t &probe) noexcept
{
    const auto it=g_v358_lifetime_probes.find(backing);
    if(it==g_v358_lifetime_probes.end()) return false;
    const auto &r=it->second;
    if(!r.exact_data || r.certified || !r.source_name_exact ||
       r.source_map>=20u || g_v359_map_conflict[r.source_map] ||
       g_v359_seen_count[r.source_map]<2u ||
       r.source_ordinal>=operators::env_spec::k_legacy_envspec_probe_count)
        return false;
    probe=r.source_ordinal;
    if(g_v359_recovered.fetch_add(1u,std::memory_order_relaxed)==0u)
        reshade::log::message(reshade::log::level::info,
            "[DSRRL V359 GI NAME RECOVERED] source=ORIGINAL_RETAIL_TLS"
            " mapping=342_EXACT_BINDER_ROWS known_sha_name_anchors=2_DISTINCT"
            " native_backing=EXACT_LIVE_D3D11 owner_gates=UNCHANGED pixel=OPEN");
    return true;
}
#endif
'''
replace(p,"std::atomic<std::uint64_t> g_v358_lifetime_next{1u};",
"std::atomic<std::uint64_t> g_v358_lifetime_next{1u};\n"+helper,
"map attestation")
replace(p,"        g_v358_lifetime_probes.clear();",
'''        g_v358_lifetime_probes.clear();
#if defined(DSRRL_V359_ATTESTED_GI_NAME_ALIAS)
        g_v359_seen_ordinal = {};
        g_v359_seen_count = {};
        g_v359_map_conflict = {};
#endif''',"reset per device")

replace(p,"""    const auto probe =
        env::legacy_native_probe_for_sha(
            digest);
#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)""",
"""    const auto probe =
        env::legacy_native_probe_for_sha(
            digest);
#if defined(DSRRL_V359_ATTESTED_GI_NAME_ALIAS)
    std::uint16_t v359_source_ordinal=0u;
    std::uint8_t v359_source_map=0u;
    const wchar_t *v359_name=nullptr;
    std::size_t v359_name_len=0u;
    const bool v359_source_exact=
        texture_identity_transport::snapshot_raw(
            v359_name,v359_name_len) &&
        v359_match_original_gi_name(
            v359_name,v359_name_len,
            v359_source_ordinal,v359_source_map);
#endif
#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)""","exact scoped producer")

replace(p,"""            item.probe=probe.has_value() ? probe.value() : 0u;
            g_v358_lifetime_probes[resource_handle.handle]=item;""",
"""            item.probe=probe.has_value() ? probe.value() : 0u;
#if defined(DSRRL_V359_ATTESTED_GI_NAME_ALIAS)
            item.source_name_exact=v359_source_exact;
            item.source_ordinal=v359_source_ordinal;
            item.source_map=v359_source_map;
            if(v359_source_exact && v359_source_map<20u &&
                probe.has_value()) {
                if(probe.value()!=v359_source_ordinal) {
                    // Contradictory native atlas vs retail binder position
                    // invalidates ALL new name-based fallback in this map.
                    g_v359_map_conflict[v359_source_map]=true;
                } else {
                    auto &mask=g_v359_seen_ordinal[v359_source_map]
                        [v359_source_ordinal/64u];
                    const auto bit=1ull<<(v359_source_ordinal%64u);
                    if((mask & bit)==0u) {
                        mask|=bit;
                        ++g_v359_seen_count[v359_source_map];
                    }
                }
            }
#endif
            g_v358_lifetime_probes[resource_handle.handle]=item;""","positive independent name-SHA evidence")

replace(p,"""    const bool ready=same_device && it!=g_native_resources.end();
    if (ready) probe=it->second.probe_ordinal;""",
"""    bool ready=same_device && it!=g_native_resources.end();
    if (ready) probe=it->second.probe_ordinal;
#if defined(DSRRL_V359_ATTESTED_GI_NAME_ALIAS)
    if(!ready && same_device)
        ready=v359_verified_native_name_probe_locked(
            static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(backing)),probe);
#endif""","native t12/t14 fallback")

print("V359_BOUNDED_SOURCE_NAME_OPERATOR_BRIDGE_MATERIALIZED")
