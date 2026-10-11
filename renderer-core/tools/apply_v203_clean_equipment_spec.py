#!/usr/bin/env python3
"""Clean v2.0.3-dev equipment-only source overlay; exact parent and one consumer edit."""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = "6a73033f6275f88487effeb1e34bbbdd7a6a3a88"
TARGET = "renderer-core/src/runtime/material_resource_draw_runtime.cpp"
HEADER = '#include "dsrrl/runtime/generated_spec_routes_v12.hpp"'
NEW_HEADER = HEADER + '\n#if defined(DSRRL_V203_EXACT_EQUIPMENT_SPEC)\n#include "dsrrl/runtime/v203_equipment_flver_slot_spec_manifest.hpp"\n#endif\n#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)\n#include "dsrrl/runtime/v203_equipment_envspec36_gate.hpp"\n#endif'
OLD = ("        const bool exact_companion =\n"
       "            h1 != 0u &&\n"
       "            generated::\n"
       "                spec_equipment_name_hash_allowed_v12(\n"
       "                    h1) &&\n"
       "            replacement != nullptr;")
NEW = OLD[:-1] + ("\n#if defined(DSRRL_V203_EXACT_EQUIPMENT_SPEC)\n"
       "            && (query.material.route_index == 345u ||\n"
       "#if defined(DSRRL_V203_CONSTEXPR_OWNER_LOOKUP)\n"
       "                v203_equipment::constexpr_lookup::\n"
       "                    exact_proven_slot_spec_hash_fast(query,h1)\n"
       "#else\n"
       "                v203_equipment::exact_proven_slot_spec_hash(query,h1)\n"
       "#endif\n"
       "            )\n"
       "#endif\n"
       "            ;")

# For eligible 36-tuples the certified EnvSpec island (not generic MR)
# consumes PTDE SpecRGB. Other materials preserve the old SpecRGB classifier.
CONSUMER_ANCHOR = "        context_spec.exact_name_ptde_companion_verified =\n"
CONSUMER_GATE = """#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)
        if (v203_equipment::exact_envspec36_material(query.material)) {
            context_spec.material_specular_consumer_verified =
                operators::material_response::classify_mtd_semantic(
                    query,
                    operators::material_response::
                        mtd_semantic_operator::env_spec).state ==
                operators::material_response::mtd_semantic_state::use;
        }
#endif
"""

ROUTER_ANCHOR = """        const auto decision =
            operators::resource_bridges::
                evaluate_spec_rgb_route(
                    context_spec,
                    query);"""
ROUTER_PATCH = """#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)
        // Dedicated EnvSpec-owned PTDE t10 shader island is not the generic
        // MR SpecRGB/PBL consumer. The 36-owner FLVER+slot+MTD+gSpec predicate
        // and exact raw-MTD EnvSpec=USE replace ONLY that generic census bit.
        // All other routes retain evaluate_spec_rgb_route(context, query).
        const bool exact_equipment_envspec_consumer =
            receiver_id >= 33u && receiver_id <= 35u &&
            spec_rgb_consumer_ready &&
            v203_equipment::exact_envspec36_material(query.material) &&
#if defined(DSRRL_V203_CONSTEXPR_OWNER_LOOKUP)
            v203_equipment::constexpr_lookup::
                exact_proven_slot_spec_hash_fast(query, h1) &&
#else
            v203_equipment::exact_proven_slot_spec_hash(query, h1) &&
#endif
            context_spec.material_specular_consumer_verified;
        const auto decision =
            exact_equipment_envspec_consumer
                ? operators::resource_bridges::
                      evaluate_spec_rgb_route(context_spec)
                : operators::resource_bridges::
                      evaluate_spec_rgb_route(context_spec, query);
#else
""" + ROUTER_ANCHOR + """
#endif"""

PROBE_ANCHOR = "    const auto h2 = hashes[2];\n\n"
PROBE = """
#if defined(DSRRL_V203_EXACT_EQUIPMENT_SPEC)
    if (receiver_id >= 24u && receiver_id <= 35u &&
        query.material.route_index != 345u) {
        static std::atomic_bool reached{false};
        if (!reached.exchange(true, std::memory_order_relaxed))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL V203 EQUIPMENT] stage=mr_resource_entry "
                "consumer=UNCHANGED");
        const auto owner_probe =
            v203_equipment::classify_exact_owner_slot(query);
#if defined(DSRRL_V203_OWNER_PATH_RUNTIME_DIAG)
        // Bounded per-family trace only; no owner/consumer decision changes.
        static std::atomic<std::uint32_t> trace_seen[3]{};
        const auto sem = query.material.semantic_name_hash;
        const auto leather =
            operators::material_response::mtd_semantic_hash(
                "P_Leather[DSB].mtd");
        const auto dull =
            operators::material_response::mtd_semantic_hash(
                "P_DullLeather[DSB].mtd");
        const unsigned group = sem == leather ? 1u
                             : sem == dull ? 2u : 0u;
        auto &counter = trace_seen[group];
        if (counter.load(std::memory_order_relaxed) < 12u &&
            counter.fetch_add(1u, std::memory_order_relaxed) < 12u) {
            char detail[640]{};
            const auto &m = query.material;
            std::snprintf(detail,sizeof(detail),
                "[DSRRL V203 OWNER TRACE] group=%u rx=%u route=%u "
                "probe=%u valid=%u actual=%u owner=%u slotvalid=%u "
                "slot=%u semantic=%016llx family=%016llx "
                "raw4=%02x%02x%02x%02x flver4=%02x%02x%02x%02x "
                "t1_hash=%016llx spec_companion=%u",
                group,
                static_cast<unsigned>(receiver_id),
                static_cast<unsigned>(m.route_index),
                static_cast<unsigned>(owner_probe),
                m.valid ? 1u : 0u,
                m.actual_material_exact ? 1u : 0u,
                m.owner_tuple_exact ? 1u : 0u,
                m.material_slot_valid ? 1u : 0u,
                static_cast<unsigned>(m.material_slot),
                static_cast<unsigned long long>(m.semantic_name_hash),
                static_cast<unsigned long long>(m.material_family_hash),
                static_cast<unsigned>(m.raw_mtd_sha256[0]),
                static_cast<unsigned>(m.raw_mtd_sha256[1]),
                static_cast<unsigned>(m.raw_mtd_sha256[2]),
                static_cast<unsigned>(m.raw_mtd_sha256[3]),
                static_cast<unsigned>(m.flver_sha256[0]),
                static_cast<unsigned>(m.flver_sha256[1]),
                static_cast<unsigned>(m.flver_sha256[2]),
                static_cast<unsigned>(m.flver_sha256[3]),
                static_cast<unsigned long long>(h1),
                companions[1] != nullptr ? 1u : 0u);
            reshade::log::message(
                reshade::log::level::info, detail);
        }
#endif
        static std::atomic_bool owner_auth_missing{false};
        if (owner_probe ==
                v203_equipment::owner_slot_probe::authority_missing &&
            !owner_auth_missing.exchange(true,std::memory_order_relaxed))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL V203 EQUIPMENT] stage=owner_authority_missing "
                "stock_dsr=ON");
        static std::atomic_bool outside_corpus_seen{false};
        if (owner_probe ==
                v203_equipment::owner_slot_probe::flver_not_in_corpus &&
            !outside_corpus_seen.exchange(true,std::memory_order_relaxed))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL V203 EQUIPMENT] stage=flver_not_in_corpus "
                "stock_dsr=ON");
        static std::atomic_bool slot_miss_seen{false};
        if (owner_probe ==
                v203_equipment::owner_slot_probe::slot_not_in_corpus &&
            !slot_miss_seen.exchange(true,std::memory_order_relaxed))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL V203 EQUIPMENT] stage=slot_not_in_corpus "
                "stock_dsr=ON");
        static std::atomic_bool mtd_miss_seen{false};
        if (owner_probe ==
                v203_equipment::owner_slot_probe::mtd_not_in_corpus &&
            !mtd_miss_seen.exchange(true,std::memory_order_relaxed))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL V203 EQUIPMENT] stage=mtd_not_in_corpus "
                "stock_dsr=ON");
        static std::atomic_bool matched{false};
        if (!matched.load(std::memory_order_relaxed) &&
            v203_equipment::exact_proven_owner_slot(query) &&
            !matched.exchange(true,std::memory_order_relaxed))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL V203 EQUIPMENT] stage=flver_slot_mtd_exact");
        static std::atomic_bool t1_seen{false};
        if (!t1_seen.load(std::memory_order_relaxed) &&
            v203_equipment::exact_proven_slot_spec_hash(query,h1) &&
            !t1_seen.exchange(true,std::memory_order_relaxed))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL V203 EQUIPMENT] stage=stock_t1_exact");
        static std::atomic_bool t1_missing{false};
        if (!t1_missing.load(std::memory_order_relaxed) &&
            v203_equipment::exact_proven_owner_slot(query) &&
            !v203_equipment::exact_proven_slot_spec_hash(query,h1) &&
            !t1_missing.exchange(true,std::memory_order_relaxed))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL V203 EQUIPMENT] stage=stock_t1_unresolved "
                "stock_bind=UNCHANGED");
        static std::atomic_bool companion_seen{false};
        if (!companion_seen.load(std::memory_order_relaxed) &&
            companions[1] != nullptr &&
            v203_equipment::exact_proven_slot_spec_hash(query,h1) &&
            !companion_seen.exchange(true,std::memory_order_relaxed))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL V203 EQUIPMENT] stage=ptde_companion_ready "
                "consumer=UNCHANGED pixel=OPEN");

    }
#endif
"""
def run(write: bool):
    source = subprocess.check_output(
        ["git", "show", BASE + ":" + TARGET], cwd=ROOT
    ).decode("utf-8").replace("\r\n", "\n")
    assert source.count(HEADER) == 1
    assert source.count(OLD) == 1
    assert "SPC25" not in source
    assert source.count(PROBE_ANCHOR) == 1
    assert source.count(CONSUMER_ANCHOR) == 1
    assert source.count(ROUTER_ANCHOR) == 1
    patched = source.replace(HEADER, NEW_HEADER, 1).replace(OLD, NEW, 1)
    patched = patched.replace(PROBE_ANCHOR, PROBE_ANCHOR + PROBE, 1)
    patched = patched.replace(CONSUMER_ANCHOR, CONSUMER_GATE + CONSUMER_ANCHOR, 1)
    patched = patched.replace(ROUTER_ANCHOR, ROUTER_PATCH, 1)
    path = ROOT / TARGET
    current = path.read_text(encoding="utf-8").replace("\r\n", "\n")
    if write:
        if current not in (source, patched):
            raise RuntimeError("Not the clean v2.0.3-dev material resource source")
        if current == source:
            path.write_text(patched, encoding="utf-8", newline="")
    elif current != patched:
        raise RuntimeError("Equipment overlay verification failed")
    # This is the ORIGINAL row-zero and LightBank-fallback source, not
    # the other-metal v203 PR296 descendant.
    pm = (ROOT / "renderer-core/src/runtime/pmetal_env_source_runtime.cpp").read_text(
        encoding="utf-8").replace("\r\n", "\n")
    if "if (latest_hook_source(out))" not in pm:
        raise RuntimeError("Required corrected v203 source fallback missing")
    if "exact_dsr_only_endpoint(" not in pm:
        raise RuntimeError("Required zero-selector exclusion missing")
    print("V203_EQUIPMENT_ONLY_" + ("APPLIED" if write else "VERIFIED"))
    print("V203_ZERO_SELECTOR_AND_LIGHTBANK_FALLBACK_PRESERVED")

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    run(ap.parse_args().apply)
