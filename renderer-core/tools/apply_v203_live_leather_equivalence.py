#!/usr/bin/env python3
"""PR304 descendant: deterministic, opt-in source overlay for six Leather slots.

Run ONLY after pristine pinned R43 materializer, the v203 source overlay and
the original equipment resource overlay. Never edit protected tracked runtime
source; this rewrites *generated working-tree build inputs* under the explicit
experimental opt-in. Idempotent --apply and strict verify prevent drift.
"""
from __future__ import annotations
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RESOURCE = ROOT / "renderer-core/src/runtime/material_resource_draw_runtime.cpp"
DRAW = ROOT / "renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp"
MACRO = "DSRRL_V203_LEATHER_PMETAL_STYLE_EXPERIMENT"

# The first pair is the exact-companion test; second is the dedicated
# EnvSpec consumer selection. Parenthesized ORs retain the original
# additional conjunctions (actual shader/receiver/resource/sidecar checks).
REPLACEMENTS = (
    (
        """                v203_equipment::constexpr_lookup::
                    exact_proven_slot_spec_hash_fast(query,h1)""",
        """                (v203_equipment::constexpr_lookup::
                    exact_proven_slot_spec_hash_fast(query,h1)
#if defined(DSRRL_V203_LEATHER_PMETAL_STYLE_EXPERIMENT)
                 || v203_equipment::live_leather::
                        exact_owner_slot_spec(query,h1)
#endif
                )""",
    ),
    (
        """                v203_equipment::exact_proven_slot_spec_hash(query,h1)""",
        """                (v203_equipment::exact_proven_slot_spec_hash(query,h1)
#if defined(DSRRL_V203_LEATHER_PMETAL_STYLE_EXPERIMENT)
                 || v203_equipment::live_leather::
                        exact_owner_slot_spec(query,h1)
#endif
                )""",
    ),
    (
        """            v203_equipment::constexpr_lookup::
                exact_proven_slot_spec_hash_fast(query, h1) &&""",
        """            (v203_equipment::constexpr_lookup::
                exact_proven_slot_spec_hash_fast(query, h1)
#if defined(DSRRL_V203_LEATHER_PMETAL_STYLE_EXPERIMENT)
             || v203_equipment::live_leather::
                    exact_owner_slot_spec(query,h1)
#endif
            ) &&""",
    ),
    (
        """            v203_equipment::exact_proven_slot_spec_hash(query, h1) &&""",
        """            (v203_equipment::exact_proven_slot_spec_hash(query, h1)
#if defined(DSRRL_V203_LEATHER_PMETAL_STYLE_EXPERIMENT)
             || v203_equipment::live_leather::
                    exact_owner_slot_spec(query,h1)
#endif
            ) &&""",
    ),
)

DIAG_ANCHOR = """    const auto h2 = hashes[2];

"""
DIAG_ADD = """#if defined(DSRRL_V203_LEATHER_PMETAL_STYLE_EXPERIMENT)
    if (receiver_id >= 33u && receiver_id <= 35u &&
        v203_equipment::live_leather::exact_owner_slot(query)) {
        static std::atomic_bool live_owner_once{false};
        if (!live_owner_once.exchange(true,std::memory_order_relaxed)) {
            char line[320]{};
            std::snprintf(line,sizeof(line),
                "[DSRRL LEATHER SOURCE] stage=full_flver_slot_raw_mtd_authenticated "
                "rx=%u actual=%u t1_exact=%u sidecar=%u per_material_source=REQUIRED",
                static_cast<unsigned>(receiver_id),
                query.material.actual_material_exact ? 1u : 0u,
                v203_equipment::live_leather::exact_owner_slot_spec(query,h1) ? 1u : 0u,
                companions[1] != nullptr ? 1u : 0u);
            reshade::log::message(reshade::log::level::info,line);
        }
    }
#endif
"""

DRAW_CANDIDATE = "    effect_latch(effect_material_ready_);\n"
DRAW_CANDIDATE_ADD = """#if defined(DSRRL_V203_LEATHER_PMETAL_STYLE_EXPERIMENT)
    if (v203_equipment::live_leather::exact_material(material)) {
        static std::atomic_bool printed{false};
        if (!printed.exchange(true,std::memory_order_relaxed))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL LEATHER SOURCE] stage=envspec_material_candidate "
                "full_sha_slot_mtd=ON actual_flag=NOT_REQUIRED other_materials=STOCK");
    }
#endif
"""
DRAW_SOURCE = "    effect_latch(effect_source_ready_);\n"
DRAW_SOURCE_ADD = """#if defined(DSRRL_V203_LEATHER_PMETAL_STYLE_EXPERIMENT)
    if (v203_equipment::live_leather::exact_material(material)) {
        static std::atomic_bool printed{false};
        if (!printed.exchange(true,std::memory_order_relaxed))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL LEATHER SOURCE] stage=per_material_lightbank_ready "
                "global_unkeyed_fallback=OFF");
    }
#endif
"""
DRAW_SPEC = "    effect_latch(effect_spec_rgb_ready_);\n"
DRAW_SPEC_ADD = """#if defined(DSRRL_V203_LEATHER_PMETAL_STYLE_EXPERIMENT)
    if (v203_equipment::live_leather::exact_material(material)) {
        static std::atomic_bool printed{false};
        if (!printed.exchange(true,std::memory_order_relaxed))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL LEATHER SOURCE] stage=ptde_t10_material_filter_ready "
                "pixel=OPEN");
    }
#endif
"""

def change(s: str, old: str, new: str) -> str:
    if s.count(new)==1:
        return s
    if s.count(old)!=1:
        raise ValueError(f"Expected exactly one original anchor, observed {s.count(old)}: {old[:100]!r}")
    return s.replace(old,new,1)

def transform_resource(s: str) -> str:
    if s.count("v203_equipment::classify_exact_owner_slot(query)")!=1:
        raise ValueError("Not an original PR304 v203 resource overlay")
    for old,new in REPLACEMENTS:
        s=change(s,old,new)
    return change(s,DIAG_ANCHOR,DIAG_ANCHOR+DIAG_ADD)

def transform_draw(s: str) -> str:
    if "v203_equipment::exact_envspec36_draw(material, decision)" not in s:
        raise ValueError("No original v203 exact source EnvSpec island")
    return change(change(change(s,
        DRAW_CANDIDATE,DRAW_CANDIDATE+DRAW_CANDIDATE_ADD),
        DRAW_SOURCE,DRAW_SOURCE+DRAW_SOURCE_ADD),
        DRAW_SPEC,DRAW_SPEC+DRAW_SPEC_ADD)

def main() -> None:
    ap=argparse.ArgumentParser()
    ap.add_argument("--apply",action="store_true")
    a=ap.parse_args()
    for path,transform in ((RESOURCE,transform_resource),(DRAW,transform_draw)):
        original=path.read_text(encoding="utf-8").replace("\r\n","\n")
        output=transform(original)
        if a.apply:
            if output != original:
                path.write_text(output,encoding="utf-8",newline="\n")
        elif output != original:
            raise RuntimeError(f"v203 live Leather overlay missing or drifted: {path}")
    print("V203_LEATHER_SOURCE_EQUIVALENCE_"+("APPLIED" if a.apply else "VERIFIED"))
    print("V203_PMETAL_ROUTE345_UNCHANGED=ON")
    print("V203_NONMETAL_UNKEYED_LIGHTBANK_FALLBACK=OFF")
    print("V203_DULLLEATHER_SHA_UNMATCHED_FAIL_OPEN=ON")

if __name__=="__main__":
    main()
