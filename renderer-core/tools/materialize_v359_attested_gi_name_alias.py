#!/usr/bin/env python3
"""V359: exact retail GI source-name recovery, guarded by independent SHA/name
attestation of at least two distinct probes on the SAME map.
Authoritative source: DSR GI packed binder 342 rows and R15 342 probe names.
No guesses from ordering at runtime, MTD, shader family or nearby resource.
"""
from pathlib import Path
import hashlib,base64,zlib,struct
root=Path(__file__).resolve().parents[1]
packed="eNod01tvE0cYBuD5dtd2ssS7Y+cckjghCSQBWgQUVW3VC0RDC1RFSJzFUSBOF/3bbQEB6kWlqkU8G62em/GO533ns+tUpDpVDJhmjgW2OM8FvuN7DrjC1VTHH2Udf/IXb3nHez7wkU/8zT/8y3/8z2eKqo4BU9TM0JIZs8AiSyyzwhoTNtlmnx+5xE9c5irX+JXfuM4NbnKL29zhLvd4wEMe8ZgnPOVZVRcp1UXgfooSd1T06OOuiincV3GEGVoyI2ZZS0dSIigoqejRZ8AU09R0zwxDGloyI8bMMsc8CyyyxDIrHGWV7tx1JmywyTG22GaH45xglz32nZwICkoq+kzYYJMtundPcZqv+JofuMglLvNzmgnfFb4jWuyLk3g/zuDd8G4c8ICHPOIxT3jGc17wkle85g2/u6GgoKSiRz8NnTkM62Hd2cOwboZDMxya4dAMGz0b+xv7G/sb+5uwZm9jb2NvE91ajz4DkwgKSip69Ok+m2Ka+nBi2jKkoXsyI8bMMsc8CyyyxDIrHGWVNdaZsMEmx9himx2Oc4Ld1Mrfyt/K38rfyt/K38rfdvlDxpAv5Av5Qr6Qz2zakC/kC/lCvpAv5Av5Qr6QL+QL+UKmkClkMtM2ZIju/D0tE0FBSUWPPvXhLzm7m6x/1j/rn/XP+mf9s/5Z/6x31jvrnfXOeme9s95Z15y6s85yjvN8wwW+5aKbTgQlg8N/z8g8RofPLHPMs8wKq6wzYYMddjnFWQ74hevc475JJoKSKo3d/zhW2WWP/fQFzldHUQ=="
raw=zlib.decompress(base64.b64decode(packed))
assert len(raw)==1368 and hashlib.sha256(raw).hexdigest()=="e25dccb8353fa57e51a23c25c246847080036e1009d56011fab7d6fa1ef7463b"
triples=[struct.unpack_from("<BBH",raw,i) for i in range(0,len(raw),4)]
names=[f"GI_EnvSpc_m{m:02d}_{a:02d}_{probe:04d}_0" for m,a,probe in triples]
assert hashlib.sha256("\n".join(names).encode()).hexdigest()=="3c92de7e08b5bd4291ec683dcf5d36ff5011a4fea7c60c38dacdfd179145325a"
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
replace(p,'#include "dsrrl/runtime/envspec_resource_runtime.hpp"',
'#include "dsrrl/runtime/envspec_resource_runtime.hpp"\n#include "dsrrl/operators/env_spec/generated_gi_name_binder_v359.hpp"\n#include "dsrrl/runtime/texture_identity_transport.hpp"',"include")
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

if "DSRRL_V359_T11_CREATION_SHA_WITNESS" in (root/p).read_text("utf8"):
    # V359 creation telemetry restructures the diffuse digest resolution.
    # Keep GI name provenance at the same semantic cut before V358 lifetime census.
    replace(p,"""    }
#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)
    if (spec) {""",
    """    }
#if defined(DSRRL_V359_ATTESTED_GI_NAME_ALIAS)
    std::uint16_t v359_source_ordinal=0u;
    std::uint8_t v359_source_map=0u;
    const wchar_t *v359_name=nullptr;
    std::size_t v359_name_len=0u;
    const bool v359_source_exact=spec &&
        texture_identity_transport::snapshot_raw(
            v359_name,v359_name_len) &&
        v359_match_original_gi_name(
            v359_name,v359_name_len,
            v359_source_ordinal,v359_source_map);
#endif
#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)
    if (spec) {""","exact scoped producer V359 t11 creation witness")
else:
    replace(p,"""        probe = env::unique_native_diffuse_probe(g_native_diffuse_hashes,digest);
        }
    #if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)""",
    """        probe = env::unique_native_diffuse_probe(g_native_diffuse_hashes,digest);
        }
    #if defined(DSRRL_V359_ATTESTED_GI_NAME_ALIAS)
        std::uint16_t v359_source_ordinal=0u;
        std::uint8_t v359_source_map=0u;
        const wchar_t *v359_name=nullptr;
        std::size_t v359_name_len=0u;
        const bool v359_source_exact=spec &&
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
