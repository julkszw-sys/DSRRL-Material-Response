#!/usr/bin/env python3
"""Clean v2.0.3-dev equipment-only source overlay; exact parent and one consumer edit."""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = "6a73033f6275f88487effeb1e34bbbdd7a6a3a88"
TARGET = "renderer-core/src/runtime/material_resource_draw_runtime.cpp"
HEADER = '#include "dsrrl/runtime/generated_spec_routes_v12.hpp"'
NEW_HEADER = HEADER + '\n#if defined(DSRRL_V203_EXACT_EQUIPMENT_SPEC)\n#include "dsrrl/runtime/v203_equipment_flver_slot_spec_manifest.hpp"\n#endif'
OLD = ("        const bool exact_companion =\n"
       "            h1 != 0u &&\n"
       "            generated::\n"
       "                spec_equipment_name_hash_allowed_v12(\n"
       "                    h1) &&\n"
       "            replacement != nullptr;")
NEW = OLD[:-1] + ("\n#if defined(DSRRL_V203_EXACT_EQUIPMENT_SPEC)\n"
       "            && (query.material.route_index == 345u ||\n"
       "                v203_equipment::exact_proven_slot_spec_hash(query,h1))\n"
       "#endif\n"
       "            ;")

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
    patched = source.replace(HEADER, NEW_HEADER, 1).replace(OLD, NEW, 1)
    patched = patched.replace(PROBE_ANCHOR, PROBE_ANCHOR + PROBE, 1)
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
