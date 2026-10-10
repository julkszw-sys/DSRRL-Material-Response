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
        (!material_ready || !prepared.material_resources.spec_rgb) &&
        material_resources_.try_recover_exact_bound_spec_from_native_name(
            context,false,true)) {
        material_resources_.release_prepared_draw(
            prepared.material_resources);
        material_ready = material_resources_.prepare_draw_requests(
            context,decision.receiver_id,query,true,true,
            prepared.material_resources);
    }
#endif
""" + C
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
 patch(ROOT/"renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp",C,D,v.apply)
