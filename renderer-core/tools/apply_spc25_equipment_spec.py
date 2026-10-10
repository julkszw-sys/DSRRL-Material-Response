#!/usr/bin/env python3
from pathlib import Path
import argparse
ROOT=Path(__file__).resolve().parents[2]
A="        const bool resource_named =\n            snapshot_native_exact_debug_name(\n                resource, resource_name, &resource_label_seen);"
B=A+"""
#if defined(DSRRL_EXPERIMENTAL_SPC25_EQUIPMENT_SPEC_BRIDGE)
        if (exact_equipment_source_test && !view_name &&
            !resource_named && !view_label && !resource_label_seen) {
            std::array<char,65u> buf{};
            if (texture_identity_transport::
                resolve_exact_equipment_native_ps_t1(
                    stock,resource,buf.data(),buf.size())) {
                try {
                    for(char c:buf) {
                        if(!c) break;
                        name.push_back(static_cast<wchar_t>(
                            static_cast<unsigned char>(c)));
                    }
                } catch(...) { name.clear(); }
                if(!generated::spec_equipment_name_hash_allowed_v12(
                    fnv_name(name))) name.clear();
            }
        }
#else
        (void)exact_equipment_source_test;
#endif"""
C="#if defined(DSRRL_EXPERIMENTAL_OTHER_METAL_PTDE_ENVSPEC)\n    if ((!material_ready || !prepared.material_resources.spec_rgb) &&\n        experimental_material && spc_onepass_claim(material, 3u)) {"
D="""#if defined(DSRRL_EXPERIMENTAL_SPC25_EQUIPMENT_SPEC_BRIDGE)
    if (experimental_material &&
        decision.receiver_id >= 33u && decision.receiver_id <= 35u &&
        material.owner_tuple_exact && material.actual_material_exact &&
        material.material_slot_valid &&
        mr::has_exact_flver_material_ownership(query) &&
        (!material_ready || !prepared.material_resources.spec_rgb)) {
        static std::atomic_bool owner_seen{false};
        if (!owner_seen.exchange(true,std::memory_order_relaxed))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL SPC25 EQUIPMENT] stage=exact_owner_candidate "
                "asset_slot_spec=CHECKING t10=UNVERIFIED pixel=OPEN");
        // Never infer a source from the MTD or from an NPC proxy texture.
        ID3D11ShaderResourceView *bound = nullptr;
        context->PSGetShaderResources(1u,1u,&bound);
        bool source_verified = false;
        bool slot_verified = false;
        if (bound) {
            ID3D11Resource *res = nullptr;
            bound->GetResource(&res);
            std::array<char,65u> name{};
            if (res) {
                source_verified = texture_identity_transport::
                    resolve_exact_equipment_native_ps_t1(
                        bound,res,name.data(),name.size());
                res->Release();
            }
            if (source_verified)
                slot_verified = spc25_equipment::exact_proven_slot_spec(
                    query,name.data());
            bound->Release();
        }
        if (source_verified) {
            static std::atomic_bool source_seen{false};
            if (!source_seen.exchange(true,std::memory_order_relaxed))
                reshade::log::message(reshade::log::level::info,
                    "[DSRRL SPC25 EQUIPMENT] stage=live_equipment_ps_t1 "
                    "cpu_source_epoch=EXACT slot_spec=CHECKING pixel=OPEN");
        }
        if (slot_verified) {
            static std::atomic_bool slot_seen{false};
            if (!slot_seen.exchange(true,std::memory_order_relaxed))
                reshade::log::message(reshade::log::level::info,
                    "[DSRRL SPC25 EQUIPMENT] stage=flver_slot_gspec "
                    "full_flver_sha=EXACT slot_mtd_gspec=EXACT pixel=OPEN");
        }
        if (slot_verified &&
            material_resources_.try_recover_exact_bound_spec_from_native_name(
                context,false,true)) {
            material_resources_.release_prepared_draw(
                prepared.material_resources);
            material_ready = material_resources_.prepare_draw_requests(
                context,decision.receiver_id,query,true,true,
                prepared.material_resources);
            if (material_ready && prepared.material_resources.spec_rgb) {
                static std::atomic_bool ready_seen{false};
                if (!ready_seen.exchange(true,std::memory_order_relaxed))
                    reshade::log::message(reshade::log::level::info,
                        "[DSRRL SPC25 EQUIPMENT] stage=ptde_t10_request_ready "
                        "stock_t1_preserved=1 pixel=OPEN");
            }
        }
    }
#endif
""" + C
I='#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"'
J=I+"""
#if defined(DSRRL_EXPERIMENTAL_SPC25_EQUIPMENT_SPEC_BRIDGE)
#include "dsrrl/runtime/texture_identity_transport.hpp"
#include "dsrrl/runtime/spc25_equipment_flver_slot_spec_manifest.hpp"
#endif"""

G="    // Limit the experiment to two exact lookups"
H="""#if defined(DSRRL_EXPERIMENTAL_SPC25_EQUIPMENT_SPEC_BRIDGE)
    if (exact_equipment_source_test) {
        ID3D11Resource *res = nullptr;
        stock->GetResource(&res);
        char name[65]{};
        bool ok = res && texture_identity_transport::
            resolve_exact_equipment_native_ps_t1(stock,res,name,sizeof(name));
        if (res) res->Release();
        if (!ok) { stock->Release(); return false; }
    }
#endif
    // Limit the experiment to two exact lookups"""
E="#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)\n    // Shared remediation for ALL exact SPC25 receiver/material profiles."
F="#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH) && \\\n    !defined(DSRRL_EXPERIMENTAL_SPC25_EQUIPMENT_SPEC_BRIDGE)\n    // The equipment test uses only the exact FLVER/slot/spec authority cut.\n    // Shared generic late-t1 recovery must not run ahead of it.\n    // Shared remediation for ALL exact SPC25 receiver/material profiles."
WRITER_A="    if(count <= 24u || (count & (count-1u)) == 0u) {"
WRITER_B="#if defined(DSRRL_EXPERIMENTAL_SPC25_EQUIPMENT_SPEC_BRIDGE)\n    // Preserve names past the global first-24 writer log limit: actual\n    // equipment-prefix names only, never infer ownership or SRV authority.\n    const auto lower_ascii = [](wchar_t c) noexcept {\n        return (c >= L'A' && c <= L'Z') ? wchar_t(c+32) : c;\n    };\n    const bool equipment_prefix =\n        length >= 6u && is_exact_tex2d && fields_readable &&\n        name[2] == L'_' && name[length-2u] == L'_' &&\n        lower_ascii(name[length-1u]) == L's' &&\n        ((lower_ascii(name[0])==L'b' && lower_ascii(name[1])==L'd') ||\n         (lower_ascii(name[0])==L'a' && lower_ascii(name[1])==L'm') ||\n         (lower_ascii(name[0])==L'h' && lower_ascii(name[1])==L'd') ||\n         (lower_ascii(name[0])==L'l' && lower_ascii(name[1])==L'g') ||\n         (lower_ascii(name[0])==L'w' && lower_ascii(name[1])==L'p'));\n    static std::atomic<std::uint32_t> equipment_name_observations{0u};\n    const auto equipment_sample = equipment_prefix ?\n        equipment_name_observations.fetch_add(\n            1u,std::memory_order_relaxed)+1u : 0u;\n    const bool sample_equipment_name =\n        equipment_sample && (equipment_sample <= 8u ||\n        ((equipment_sample & (equipment_sample-1u))==0u));\n#else\n    constexpr bool sample_equipment_name = false;\n#endif\n    if(count <= 24u || (count & (count-1u)) == 0u ||\n       sample_equipment_name) {"
def patch(p,a,b,apply):
 s=p.read_text(encoding="utf-8")
 if s.count(b)==1: pass
 elif apply and s.count(a)==1: p.write_text(s.replace(a,b,1),encoding="utf-8",newline="")
 else: raise RuntimeError("equipment patch drift "+str(p))
 print("EQUIPMENT_"+("APPLIED" if apply else "VERIFIED")+" "+p.name)
if __name__=="__main__":
 ap=argparse.ArgumentParser();ap.add_argument("--apply",action="store_true");v=ap.parse_args()
 patch(ROOT/"renderer-core/src/runtime/material_resource_draw_runtime.cpp",G,H,v.apply)
 patch(ROOT/"renderer-core/src/runtime/material_resource_draw_runtime.cpp",A,B,v.apply)
 patch(ROOT/"renderer-core/src/runtime/texture_identity_transport.cpp",WRITER_A,WRITER_B,v.apply)
 patch(ROOT/"renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp",E,F,v.apply)
 patch(ROOT/"renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp",I,J,v.apply)
 patch(ROOT/"renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp",C,D,v.apply)
