#!/usr/bin/env python3
"""Opt-in V204 fixes on the reproducible PR305 generated build sources.
Run after apply_v203_full_sidecar_1295.py; never edit protected tracked
original DSRRL / P_Metal sources. Negative controls must remain stock.
"""
from pathlib import Path
import argparse

ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/"renderer-core/src/runtime"

def once(src,old,new):
    if src.count(new)==1: return src
    if src.count(old)!=1:
        raise AssertionError("V204 anchor mismatch %r count=%s" % (old[:75],src.count(old)))
    return src.replace(old,new,1)

def resource(s):
    old="""    const bool spec_member =
        generated::spec_equipment_name_hash_allowed_v12(
            logical_hash) ||
        subsurface_body_spec;"""
    new="""#if defined(DSRRL_V203_FULL_SIDECAR_1295)
    const bool spec_member =
        generated::spec_equipment_name_hash_allowed_v12(logical_hash) ||
        sidecar_20261010::spec_allowed(logical_hash) ||
        subsurface_body_spec;
#else
"""+old+"""
#endif"""
    s=once(s,old,new)
    old="""        account_load(loaded.status);
        set.specular = loaded.view;"""
    new="""        account_load(loaded.status);
#if defined(DSRRL_V203_FULL_SIDECAR_1295)
        if (sidecar_20261010::spec_allowed(logical_hash)) {
            static std::atomic<std::uint32_t> count{0u};
            if (count.fetch_add(1u,std::memory_order_relaxed)<16u) {
                char line[280]{};
                std::snprintf(line,sizeof(line),
                    "[DSRRL SIDECAR 1295] stage=resource_view_init "
                    "name=%016llx dds_status=%u ready=%u",
                    static_cast<unsigned long long>(logical_hash),
                    static_cast<unsigned>(loaded.status),
                    loaded.view!=nullptr?1u:0u);
                reshade::log::message(reshade::log::level::info,line);
            }
        }
#endif
        set.specular = loaded.view;"""
    s=once(s,old,new)
    return s

def source(s):
    # Shared LightBank producer, exact per-material selector authority.
    # P_Metal continues to use original narrow fallback; nonmetals only
    # consume their complete material/epoch keyed source publication.
    old="""        !exact_pmetal_material_selection(
            material) ||
        owner == nullptr ||"""
    new="""        !(
            exact_pmetal_material_selection(material)
#if defined(DSRRL_V203_FULL_SIDECAR_1295)
            || sidecar_20261010::exact_nonmetal_material(material)
#endif
        ) ||
        owner == nullptr ||"""
    s=once(s,old,new)
    old="""        !exact_pmetal_material_selection(
            material)) {
        telemetry::hot_count(
            g_consumer_fail);"""
    new="""        !(
            exact_pmetal_material_selection(material)
#if defined(DSRRL_V203_FULL_SIDECAR_1295)
            || sidecar_20261010::exact_nonmetal_material(material)
#endif
        )) {
        telemetry::hot_count(
            g_consumer_fail);"""
    s=once(s,old,new)
    # Explicitly forbid the unkeyed global/latest-hook LightBank fallback
    # for nonmetal. This source is not tied to the complete material key.
    old="""    // Narrow fallback carrier recovered from the attested retail LightBank
    // single/blend packers (V13 semantic cut)."""
    new="""#if defined(DSRRL_V203_FULL_SIDECAR_1295)
    if (!exact_pmetal_material_selection(material)) {
        // For nonmetals, a matching selector event must have published its
        // material+source+epoch payload. Never use global latest_hook_source.
        telemetry::hot_count(g_consumer_fail);
        return false;
    }
#endif

"""+old
    s=once(s,old,new)
    return s

def diag(s):
    # PR305 original bounded resource probe emitted twelve copies of the
    # first null t1 and hid subsequent valid/missing-cache cases.
    old="""        static std::atomic<std::uint32_t> seen{0u};
        if (seen.fetch_add(1u,std::memory_order_relaxed)<12u) {
            char line[360]{};
            std::snprintf(line,sizeof(line),
                "[DSRRL SIDECAR 1295] stage=nonmetal_resource_probe "
                "rx=%u slot=%u actual=%u fullowner=%u stock_t1=%016llx "
                "name_allowed=%u ptde_companion=%u",
                static_cast<unsigned>(receiver_id),
                static_cast<unsigned>(query.material.material_slot),
                query.material.actual_material_exact?1u:0u,
                query.material.owner_tuple_exact?1u:0u,
                static_cast<unsigned long long>(h1),
                sidecar_20261010::spec_allowed(h1)?1u:0u,
                companions[1]!=nullptr?1u:0u);"""
    new="""        // Distinguish absent stock SRV from missing logical resource registry
        // and absent PTDE companion; log first 6 of each outcome independently.
        const unsigned kind = views[1]==nullptr ? 0u :
            h1==0u ? 1u : companions[1]==nullptr ? 2u : 3u;
        static std::array<std::atomic<std::uint32_t>,4u> seen{};
        if (seen[kind].fetch_add(1u,std::memory_order_relaxed)<6u) {
            char line[440]{};
            const unsigned mask =
                (views[0]!=nullptr?1u:0u) |
                (views[1]!=nullptr?2u:0u) |
                (views[2]!=nullptr?4u:0u);
            std::snprintf(line,sizeof(line),
                "[DSRRL SIDECAR 1295] stage=nonmetal_resource_probe "
                "rx=%u slot=%u actual=%u fullowner=%u "
                "ps012=%u t1_kind=%u stock_t0=%016llx "
                "stock_t1=%016llx stock_t2=%016llx "
                "name_allowed=%u ptde_companion=%u",
                static_cast<unsigned>(receiver_id),
                static_cast<unsigned>(query.material.material_slot),
                query.material.actual_material_exact?1u:0u,
                query.material.owner_tuple_exact?1u:0u,
                mask,kind,
                static_cast<unsigned long long>(h0),
                static_cast<unsigned long long>(h1),
                static_cast<unsigned long long>(h2),
                sidecar_20261010::spec_allowed(h1)?1u:0u,
                companions[1]!=nullptr?1u:0u);"""
    s=once(s,old,new)
    return s

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--apply",action="store_true")
    a=parser.parse_args()
    for name,fn in (
        ("material_resource_draw_runtime.cpp",lambda s:diag(resource(s))),
        ("pmetal_env_source_runtime.cpp",source),
    ):
        path=SRC/name
        original=path.read_text(encoding="utf-8").replace("\r\n","\n")
        if "DSRRL_V203_FULL_SIDECAR_1295" not in original:
            raise AssertionError("V204 requires prior PR305 opt-in overlay")
        updated=fn(original)
        if a.apply and original!=updated:
            path.write_text(updated,encoding="utf-8",newline="\n")
        elif not a.apply and original!=updated:
            raise AssertionError("V204 source not applied: "+name)
    print("V204_RESOURCE_REGISTRATION_1295_PASS")
    print("V204_NONMETAL_LIGHTBANK_MATERIAL_KEYED_ONLY_PASS")
    print("V204_PMETAL_UNKEYED_FALLBACK_UNCHANGED_PASS")

if __name__=="__main__":
    main()
