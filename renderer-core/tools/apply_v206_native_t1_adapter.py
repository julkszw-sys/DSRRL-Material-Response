#!/usr/bin/env python3
"""V206: minimal SPC25 CPU-native view identity ADAPTER for exact V204 runtime.

Unlike V205 this NEVER writes/replaces texture_identity_transport.cpp from
a historical commit. Two tiny *source-local* overlays add an independently
verified engine writer hook and an RX24-35 observation using existing V204
ReShade callbacks, cache and sidecar manifest. P_Metal and PTDE t10 unchanged.

No c5330_s, NPC probe, old decoder, SPC25 batch model or unsafe GPU swap.
"""
from __future__ import annotations
import argparse
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
TEX=ROOT/"renderer-core/src/runtime/texture_identity_transport.cpp"
MAT=ROOT/"renderer-core/src/runtime/material_resource_draw_runtime.cpp"
HDR=ROOT/"renderer-core/include/dsrrl/runtime/texture_identity_transport.hpp"

def one(s: str, before: str, after: str) -> str:
    if s.count(after)==1:return s
    if s.count(before)!=1:
        raise AssertionError("V206 parent materialized source drift: "+
                             repr(before[:100])+" count="+str(s.count(before)))
    return s.replace(before,after,1)

def header(s: str) -> str:
    return one(s,"    bool clear_hook_armed = false;\n",
               "    bool clear_hook_armed = false;\n"
               "    bool cpu_writer_hook_armed = false;\n")

def transport(s: str) -> str:
    s=one(s,'extern "C" void dsrrl_texture_name_clear_hook_entry();',
          'extern "C" void dsrrl_texture_name_clear_hook_entry();\n'
          'extern "C" void dsrrl_v206_writer_hook_entry();\n'
          'extern "C" void *g_dsrrl_v206_writer_target;\n'
          'extern "C" void *g_dsrrl_v206_writer_resume;')
    s=one(s,'constexpr std::uintptr_t k_clear_rva = 0x583E81u;',
          'constexpr std::uintptr_t k_clear_rva = 0x583E81u;\n'
          'constexpr std::uintptr_t k_cpu_writer_packet_rva = 0x583BCEu;\n'
          'constexpr std::uintptr_t k_cpu_writer_rva = 0x57EFB0u;\n'
          'constexpr std::uintptr_t k_cpu_writer_resume_rva = 0x583BF2u;\n'
          'constexpr std::array<std::uint8_t,15> k_cpu_writer_bytes = {\n'
          '    0x48,0x8B,0xC8,0x4C,0x8B,0xC7,0x8B,0xD6,\n'
          '    0xE8,0xD5,0xB3,0xFF,0xFF,0xEB,0x15\n'
          '};')
    s=one(s,"hook g_name{}, g_clear{};",
          "hook g_name{}, g_clear{}, g_v206_cpu_writer{};")
    s=one(s,"    if (g_name.patched ||\n        g_clear.patched)",
          "    if (g_name.patched ||\n        g_clear.patched ||\n"
          "        g_v206_cpu_writer.patched)")
    s=one(s,'                &dsrrl_texture_name_clear_hook_entry)))\n'
            '        goto fail;',
          '                &dsrrl_texture_name_clear_hook_entry)) ||\n'
          '        !prepare_hook(g_v206_cpu_writer,\n'
          '            k_cpu_writer_packet_rva,k_cpu_writer_bytes,\n'
          '            reinterpret_cast<void *>(&dsrrl_v206_writer_hook_entry)))\n'
          '        goto fail;')
    s=one(s,'    if (!arm(g_name) ||\n        !arm(g_clear))',
          '    g_dsrrl_v206_writer_target = reinterpret_cast<void *>(\n'
          '        g_base + k_cpu_writer_rva);\n'
          '    g_dsrrl_v206_writer_resume = reinterpret_cast<void *>(\n'
          '        g_base + k_cpu_writer_resume_rva);\n'
          '    if (!arm(g_name) ||\n        !arm(g_clear) ||\n'
          '        !arm(g_v206_cpu_writer))')
    s=one(s,"    g_status.clear_hook_armed = true;\n    return true;",
          "    g_status.clear_hook_armed = true;\n"
          "    g_status.cpu_writer_hook_armed = true;\n    return true;")
    s=one(s,'    const bool clear_ok =\n        restore(g_clear);',
          '    const bool writer_ok=restore(g_v206_cpu_writer);\n'
          '    const bool clear_ok =\n        restore(g_clear);')
    s=one(s,"    if (!clear_ok || !name_ok) {",
          "    if (!writer_ok || !clear_ok || !name_ok) {")
    s=one(s,"        g_status.clear_hook_armed =\n            g_clear.patched;",
          "        g_status.clear_hook_armed =\n            g_clear.patched;\n"
          "        g_status.cpu_writer_hook_armed =\n"
          "            g_v206_cpu_writer.patched;")
    s=one(s,"    g_dsrrl_texture_name_resume = nullptr;",
          "    g_dsrrl_v206_writer_target=nullptr;\n"
          "    g_dsrrl_v206_writer_resume=nullptr;\n"
          "    g_dsrrl_texture_name_resume = nullptr;")
    return s

def resource(s: str) -> str:
    s=one(s,'#include "dsrrl/runtime/texture_identity_transport.hpp"',
          '#include "dsrrl/runtime/texture_identity_transport.hpp"\n'
          '#include "dsrrl/runtime/v206_native_t1_provenance.hpp"')
    s=one(s,"void on_init_resource_view(\n"
          "    reshade::api::device *device,\n"
          "    reshade::api::resource,\n",
          "void on_init_resource_view(\n"
          "    reshade::api::device *device,\n"
          "    reshade::api::resource source_resource,\n")
    token="    stutter_profile::scope view_time(stutter_profile::stage::resource_view_init);"
    s=one(s,token,
          "    // Full stock D3D11 lifecycle witness even when original name\n"
          "    // callback was missed. No companion is admitted by this call.\n"
          "    v206_native_t1::instance().init(\n"
          "        static_cast<std::uintptr_t>(device->get_native()),\n"
          "        static_cast<std::uintptr_t>(view.handle),\n"
          "        static_cast<std::uintptr_t>(source_resource.handle));\n"
          +token)
    s=one(s,
          "void on_destroy_resource_view(\n"
          "    reshade::api::device *,\n"
          "    reshade::api::resource_view view)\n"
          "{\n    companion_set dead{};",
          "void on_destroy_resource_view(\n"
          "    reshade::api::device *device,\n"
          "    reshade::api::resource_view view)\n"
          "{\n"
          "    if (device && device->get_api()==reshade::api::device_api::d3d11)\n"
          "        v206_native_t1::instance().destroy(\n"
          "            static_cast<std::uintptr_t>(device->get_native()),\n"
          "            static_cast<std::uintptr_t>(view.handle));\n"
          "    companion_set dead{};")
    s=one(s,
          "    release_cache_for_device(\n        native);\n}",
          "    v206_native_t1::instance().destroy_device(\n"
          "        reinterpret_cast<std::uintptr_t>(native));\n"
          "    release_cache_for_device(\n        native);\n}")
    method="bool material_resource_draw_runtime::\nprepare_draw_requests_bound("
    if s.count(method)!=1:raise AssertionError("Missing unique V204 draw consumer")
    offset=s.index(method)
    marker="    const auto h2 = hashes[2];\n"
    at=s.index(marker,offset)
    add="""#if defined(DSRRL_V206_SPC25_EQUIPMENT_ID)
    // Receiver-general DIAGNOSTIC only; existing V204 MR/SpecRGB P_Metal
    // consumer and independent 1295-sidecar FLVER/MTD policy remain unchanged.
    if (receiver_id>=24u && receiver_id<=35u &&
        query.material.owner_tuple_exact &&
        query.material.route_index!=345u &&
        views[1]!=nullptr) {
        // V207 sampled only h1==0, excluding the named/companion
        // candidates already visible in V203 OWNER TRACE.
        // V208 keeps independent budgets so stock/unknown PS t1
        // cannot starve named sidecar SRVs; diagnostic only.
        const bool known_sidecar=(h1!=0u);
        static std::atomic<std::uint32_t> sampled_known{0u};
        static std::atomic<std::uint32_t> sampled_unknown{0u};
        const auto n=(known_sidecar?sampled_known:sampled_unknown)
            .fetch_add(1u,std::memory_order_relaxed)+1u;
        if (n<=20u || (n<=8192u && (n&(n-1u))==0u)) {
            ID3D11Resource *resource=nullptr;
            views[1]->GetResource(&resource);
            if (resource) {
                const auto probe=v206_native_t1::instance().probe(
                    reinterpret_cast<std::uintptr_t>(views[1]),
                    reinterpret_cast<std::uintptr_t>(resource));
                const auto witness=probe.source;
                const auto census=v206_native_t1::instance().stats();
                const auto hook=texture_identity_transport::status();
                std::uint64_t source_hash=0ull;
                bool whitelisted=false;
                if (witness) {
                    std::array<wchar_t,65u> name{};
                    std::size_t len=0u;
                    while(len<64u && witness.logical_name[len]){
                        name[len]=static_cast<unsigned char>(
                            witness.logical_name[len]);
                        ++len;
                    }
                    source_hash=fnv_name(name.data(),len);
                    whitelisted=sidecar_20261010::spec_allowed(source_hash);
                }
                // V210 diagnostic: do NOT use these partial matches as
                // material authority. The 36-row whitelist is provenance
                // evidence, not a fallback for missing runtime actual MTD.
#if defined(DSRRL_V203_EXACT_EQUIPMENT_SPEC)
                bool name_in_36=false;
                bool flver_in_36=false;
                bool slot_in_36=false;
                bool mtd_in_36=false;
                bool tuple_with_t1_in_36=false;
                for(const auto &row:v203_equipment::k_proven_parts_specs){
                    const bool name_match=h1!=0u &&
                        v203_equipment::logical_spec_hash(row.spec)==h1;
                    name_in_36 |= name_match;
                    if(!v203_equipment::digest_matches(
                           query.material.flver_sha256,row.flver_sha256))
                        continue;
                    flver_in_36=true;
                    if(!query.material.material_slot_valid ||
                       query.material.material_slot!=row.slot)
                        continue;
                    slot_in_36=true;
                    if(query.material.semantic_name_hash!=
                       operators::material_response::mtd_semantic_hash(
                           row.mtd))
                        continue;
                    mtd_in_36=true;
                    if(name_match)tuple_with_t1_in_36=true;
                }
                char auth_line[440]{};
                std::snprintf(auth_line,sizeof(auth_line),
                    "[DSRRL V210 MATERIAL CUT] rx=%u slot=%u "
                    "h1=%016llx name36=%u flver36=%u slot36=%u "
                    "mtd36=%u full_tuple36=%u "
                    "actual_mtd=%u owner_tuple=%u "
                    "flver_mtd_owner=%u consumer_authority=0 "
                    "bridge_authority=0 swap=0 pixel=OPEN",
                    static_cast<unsigned>(receiver_id),
                    static_cast<unsigned>(query.material.material_slot),
                    static_cast<unsigned long long>(h1),
                    name_in_36?1u:0u,flver_in_36?1u:0u,
                    slot_in_36?1u:0u,mtd_in_36?1u:0u,
                    tuple_with_t1_in_36?1u:0u,
                    query.material.actual_material_exact?1u:0u,
                    query.material.owner_tuple_exact?1u:0u,
                    operators::material_response::
                        has_exact_flver_material_ownership(query)?1u:0u);
                reshade::log::message(reshade::log::level::info,auth_line);
#endif
                // V211: the actual full-sidecar predicate, not the 36-row
                // historical equipment manifest. Diagnostic only.
#if defined(DSRRL_V203_FULL_SIDECAR_1295)
                const auto &mat=query.material;
                const auto exact_query=
                    sidecar_20261010::make_query(mat,receiver_id);
                const auto env=operators::material_response::
                    classify_mtd_envspec_semantics(exact_query);
                const auto sem=operators::material_response::
                    classify_mtd_semantic(
                        exact_query,
                        operators::material_response::
                            mtd_semantic_operator::env_spec);
                const bool family_difspcbmp=
                    mat.material_family_hash==
                    operators::material_response::
                        mtd_semantic_hash("DifSpcBmp");
                const bool stock_t1_1295=
                    h1!=0u && sidecar_20261010::spec_allowed(h1);
                const bool companion_ready=companions[1]!=nullptr;
                const bool nonmetal_material=
                    sidecar_20261010::exact_nonmetal_material(mat);
                const bool complete_t1_gate=
                    sidecar_20261010::exact_nonmetal_stock_t1(
                        exact_query,h1,companion_ready);
                char cut[900]{};
                std::snprintf(cut,sizeof(cut),
                    "[DSRRL V211 EXACT NONMETAL CUT] "
                    "rx=%u slot=%u h1=%016llx "
                    "stock_t1_in_1295=%u cpu_witness_in_1295=%u "
                    "ptde_companion=%u flver_owner=%u "
                    "family_difspcbmp=%u route345=%u actual_mtd=%u "
                    "env_exact=%u env_presence=%u env_router=%u "
                    "env_slot_valid=%u env_slot=%u mtd_envspec_use=%u "
                    "nonmetal_material=%u complete_t1_gate=%u "
                    "bridge_authority=0 swap=0 pixel=OPEN",
                    static_cast<unsigned>(receiver_id),
                    static_cast<unsigned>(mat.material_slot),
                    static_cast<unsigned long long>(h1),
                    stock_t1_1295?1u:0u,
                    whitelisted?1u:0u,companion_ready?1u:0u,
                    operators::material_response::
                        has_exact_flver_material_ownership(exact_query)?1u:0u,
                    family_difspcbmp?1u:0u,
                    mat.route_index==345u?1u:0u,
                    mat.actual_material_exact?1u:0u,
                    env.exact_identity_match?1u:0u,
                    static_cast<unsigned>(env.presence),
                    static_cast<unsigned>(env.router_state),
                    env.envspc_slot_valid?1u:0u,
                    static_cast<unsigned>(env.envspc_slot),
                    sem.state==operators::material_response::
                        mtd_semantic_state::use?1u:0u,
                    nonmetal_material?1u:0u,
                    complete_t1_gate?1u:0u);
                reshade::log::message(reshade::log::level::info,cut);
#endif
                char line[640]{};
                std::snprintf(line,sizeof(line),
                    "[DSRRL V208 NATIVE T1] rx=%u slot=%u route=%u "
                    "v208_lane=%s h1=%016llx companion_present=%u "
                    "epoch=%llu engine_cpu_srv_exact=%u name=%s "
                    "in_sidecar_1295=%u live_stock_t1=%p "
                    "resource=%p v207_view_state=%u "
                    "v207_registry_init=%llu/%llu writers=%llu "
                    "v207_writer_hook_armed=%u "
                    "bridge_authority=0 swap=0 pixel=OPEN",
                    static_cast<unsigned>(receiver_id),
                    static_cast<unsigned>(query.material.material_slot),
                    static_cast<unsigned>(query.material.route_index),
                    known_sidecar?"NAMED":"UNNAMED",
                    static_cast<unsigned long long>(h1),
                    companions[1]?1u:0u,
                    static_cast<unsigned long long>(witness.epoch),
                    witness?1u:0u,
                    witness?witness.logical_name.data():"NONE",
                    whitelisted?1u:0u,
                    static_cast<void *>(views[1]),
                    static_cast<void *>(resource),
                    static_cast<unsigned>(probe.status),
                    static_cast<unsigned long long>(census.init_accepted),
                    static_cast<unsigned long long>(census.init_called),
                    static_cast<unsigned long long>(census.writer_accepted),
                    hook.cpu_writer_hook_armed?1u:0u);
                reshade::log::message(reshade::log::level::info,line);
                resource->Release();
            }
        }
    }
#endif
"""
    if "engine_cpu_srv_exact" in s[at:at+len(marker)+len(add)+50]:return s
    return s[:at+len(marker)]+add+s[at+len(marker):]

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--apply",action="store_true")
    args=ap.parse_args()
    for path,patch in [(HDR,header),(TEX,transport),(MAT,resource)]:
        original=path.read_text(encoding="utf-8").replace("\r\n","\n")
        updated=patch(original)
        if args.apply and updated!=original:
            path.write_text(updated,encoding="utf-8",newline="\n")
        elif not args.apply and updated!=original:
            raise RuntimeError("V206 missing generated exact overlay: "+str(path))
    print("V206_V204_NATIVE_T1_ADAPTER_"+("APPLIED" if args.apply else "VERIFIED"))
    print("V206_FULL_TRANSPORT_REPLACEMENT=FORBIDDEN")
    print("V206_LEGACY_NPC_C5330=ABSENT")
    print("V206_CARRIER_BINDING_UNCHANGED=ON")

if __name__=="__main__":
    main()
