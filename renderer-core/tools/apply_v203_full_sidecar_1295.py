#!/usr/bin/env python3
"""Deterministic post-materializer V203 full PTDE Specular sidecar experiment.

Never alter source tracked in Git. The PR304 reproducible Windows builder first
materializes exact protected R43 source and then its original 36-owner overlays.
This opt-in script transforms those *ephemeral built-source* files, adding a
1295-name resource whitelist and material-local, FULL FLVER owner + MTD SHA
semantic gate. No global LightBank fallback is allowed for nonmetals.
Original P_Metal route345 and its guard are unchanged. Native/pixel OPEN.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / "renderer-core/src/runtime"
INC = '#include "dsrrl/runtime/v203_sidecar_operator_gate.hpp"'
INJECTION = ('#if defined(DSRRL_V203_FULL_SIDECAR_1295)\n'
             + INC + '\n#endif\n')


def replace_once(source: str, old: str, new: str) -> str:
    if source.count(new) == 1:
        return source
    if source.count(old) != 1:
        raise AssertionError(f"Expected one exact PR304 anchor, observed {source.count(old)}: {old[:110]!r}")
    return source.replace(old, new, 1)


def include_gate(s: str) -> str:
    anchor = '#include "dsrrl/runtime/v203_equipment_envspec36_gate.hpp"\n'
    return replace_once(s, anchor, anchor + INJECTION)


def patch_resources(s: str) -> str:
    s=include_gate(s)
    marker="#if defined(DSRRL_V203_FULL_SIDECAR_1295)\n        const bool exact_companion ="
    if marker not in s:
        begin=s.find("        const bool exact_companion =\n")
        if begin<0 or s.find("        const bool exact_companion =\n",begin+1)>=0:
            raise AssertionError("Ambiguous replacement candidate")
        end=s.find("\n            ;",begin)
        if end<0: raise AssertionError("PR304 exact companion block not materialized")
        end+=len("\n            ;")
        old=s[begin:end]
        new="""#if defined(DSRRL_V203_FULL_SIDECAR_1295)
        // Original P_Metal resource policy remains byte-for-byte equivalent.
        // Nonmetals see all 1295 names from the real sidecar, not 36 slots.
        const bool exact_companion =
            h1 != 0u &&
            (query.material.route_index == 345u
              ? generated::spec_equipment_name_hash_allowed_v12(h1)
              : sidecar_20261010::spec_allowed(h1)) &&
            replacement != nullptr;
#else
"""+old+"""
#endif"""
        s=replace_once(s,old,new)
    anchor="        context_spec.exact_name_ptde_companion_verified =\n"
    addon="""#if defined(DSRRL_V203_FULL_SIDECAR_1295)
        if (sidecar_20261010::exact_nonmetal_material(query.material))
            context_spec.material_specular_consumer_verified =
                operators::material_response::classify_mtd_semantic(
                    query,
                    operators::material_response::
                        mtd_semantic_operator::env_spec).state ==
                operators::material_response::mtd_semantic_state::use;
#endif
"""
    s=replace_once(s,anchor,addon+anchor)
    anchor="#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)\n        // Dedicated EnvSpec-owned PTDE t10 shader island"
    new="""#if defined(DSRRL_V203_FULL_SIDECAR_1295)
        // A sidecar-name match is never itself a PTDE shader authorization.
        // The stock bound PS t1, matching companion, full FLVER owner,
        // exact MTD EnvSpec semantic and RX33..35 are all required.
        const bool exact_equipment_envspec_consumer =
            receiver_id >= 33u && receiver_id <= 35u &&
            spec_rgb_consumer_ready &&
            sidecar_20261010::exact_nonmetal_stock_t1(
                query, h1, exact_companion) &&
            context_spec.material_specular_consumer_verified;
        const auto decision =
            exact_equipment_envspec_consumer
              ? operators::resource_bridges::
                    evaluate_spec_rgb_route(context_spec)
              : operators::resource_bridges::
                    evaluate_spec_rgb_route(context_spec, query);
#elif defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)
        // Dedicated EnvSpec-owned PTDE t10 shader island"""
    s=replace_once(s,anchor,new)
    anchor="    const auto h2 = hashes[2];\n\n"
    diag="""#if defined(DSRRL_V203_FULL_SIDECAR_1295)
    if (receiver_id>=33u && receiver_id<=35u &&
        sidecar_20261010::exact_nonmetal_material(query.material)) {
        static std::atomic<std::uint32_t> seen{0u};
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
                companions[1]!=nullptr?1u:0u);
            reshade::log::message(reshade::log::level::info,line);
        }
    }
#endif
"""
    s=replace_once(s,anchor,anchor+diag)
    return s


def patch_source(s: str) -> str:
    s=include_gate(s)
    old="    return v203_equipment::exact_envspec36_material(material);"
    new="""#if defined(DSRRL_V203_FULL_SIDECAR_1295)
    return sidecar_20261010::exact_nonmetal_material(material);
#else
    return v203_equipment::exact_envspec36_material(material);
#endif"""
    return replace_once(s,old,new)


def patch_draw(s: str) -> str:
    s=include_gate(s)
    old="    return v203_equipment::exact_envspec36_draw(material, decision);"
    new="""#if defined(DSRRL_V203_FULL_SIDECAR_1295)
    return sidecar_20261010::exact_nonmetal_draw(material, decision);
#else
    return v203_equipment::exact_envspec36_draw(material, decision);
#endif"""
    return replace_once(s,old,new)


def patch_selector(s: str) -> str:
    s=include_gate(s)
    old="        || v203_equipment::exact_envspec36_material(identity)"
    new="""#if defined(DSRRL_V203_FULL_SIDECAR_1295)
        || sidecar_20261010::exact_nonmetal_material(identity)
#else
        || v203_equipment::exact_envspec36_material(identity)
#endif"""
    return replace_once(s,old,new)


def main() -> None:
    ap=argparse.ArgumentParser()
    ap.add_argument("--apply",action="store_true")
    args=ap.parse_args()
    cases=(
        ("material_resource_draw_runtime.cpp",patch_resources),
        ("pmetal_env_source_runtime.cpp",patch_source),
        ("pmetal_envspec_draw_runtime.cpp",patch_draw),
        ("flver_engine_hooks.cpp",patch_selector),
    )
    for filename,transform in cases:
        path=RUNTIME / filename
        original=path.read_text(encoding="utf-8").replace("\r\n","\n")
        if "DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG" not in original:
            raise AssertionError(f"Requires PR304 exact EnvSpec base: {filename}")
        updated=transform(original)
        if args.apply:
            if original!=updated:
                path.write_text(updated,encoding="utf-8",newline="\n")
        elif original!=updated:
            raise AssertionError(f"Full sidecar overlay missing or drifted: {filename}")
    print("V203_SIDECAR_1295_"+("APPLIED" if args.apply else "VERIFIED"))
    print("V203_PMETAL_ROUTE345_ORIGINAL=ON")
    print("V203_NONMETAL_SOURCE_MUST_BE_KEYED=ON")
    print("V203_DULLLEATHER_UNRESOLVED_SHA_FAIL_OPEN=ON")


if __name__=="__main__":
    main()
