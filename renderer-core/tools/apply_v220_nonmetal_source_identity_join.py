#!/usr/bin/env python3
"""V220 opt-in, exact nonmetal producer/consumer key normalization.

Applied only to the ephemeral generated source AFTER V203 full 1295,
V204 and V219 overlays. Do not edit the pinned R43 source.

R43's producer-state key includes actual_material_exact. For authenticated
nonmetal FLVER+slot+raw MTD identity the selector and draw may disagree about
that *transport hint* while still agreeing on every authoritative field.
Canonicalize the hint to false on BOTH sides of the keyed exchange, but only
for sidecar_20261010::exact_nonmetal_material (full SHA and MTD semantics).
P_Metal retains the old byte-for-byte keyed and latest_hook_source paths.

This is a conditional transport fix. A missing selector publication, a
missing probe/t1 or a missing shader consumer still fails open to stock DSR.
"""
from pathlib import Path
import argparse

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "renderer-core/src/runtime/pmetal_env_source_runtime.cpp"
ANCHOR = "bool retail_lightbank_record_index("
HELPER = """// V220: only an authenticated nonmetal may ignore the advisory
// actual_material_exact flag in an otherwise FULL source/material key.
// Keep route345 / P_Metal's R43 key, source producer and fallback untouched.
inline operators::material_response::material_identity
v220_exact_nonmetal_source_key(
    const operators::material_response::material_identity &material) noexcept
{
    if (!sidecar_20261010::exact_nonmetal_material(material))
        return material;
    auto exact = material;
    exact.actual_material_exact = false;
    return exact;
}

"""
CHANGES = (
    ("pmetal_producer_state_begin(\n        material,\n        epoch);",
     "pmetal_producer_state_begin(\n        v220_exact_nonmetal_source_key(material),\n        epoch);", 1),
    ("pmetal_producer_state_publish(\n            material,\n            next,\n            epoch);",
     "pmetal_producer_state_publish(\n            v220_exact_nonmetal_source_key(material),\n            next,\n            epoch);", 1),
    ("pmetal_producer_state_publish(\n        material,\n        next,\n        epoch);",
     "pmetal_producer_state_publish(\n        v220_exact_nonmetal_source_key(material),\n        next,\n        epoch);", 1),
    ("if (pmetal_producer_state_latest(\n            material,\n            epoch,\n            out)) {",
     """const bool v220_nonmetal_join =
        pmetal_producer_state_latest(
            v220_exact_nonmetal_source_key(material),
            epoch,
            out);

    // This is a first-N actual consumer-join witness, not permission to
    // bind any shader/resource. It distinguishes a producer join from
    // merely recognizing an RX33..35 material candidate.
    if (sidecar_20261010::exact_nonmetal_material(material)) {
        static std::atomic<std::uint32_t> seen_hit{0u};
        static std::atomic<std::uint32_t> seen_miss{0u};
        auto &seen = v220_nonmetal_join ? seen_hit : seen_miss;
        if (seen.fetch_add(1u, std::memory_order_relaxed) < 12u) {
            char line[336]{};
            std::snprintf(
                line, sizeof(line),
                "[DSRRL V220 SOURCE JOIN] route=%u slot=%u "
                "actual_mtd=%u owner_exact=1 keyed_source=%u "
                "producer_tls=%u fallback_to_PMetal=NO pixel=OPEN",
                static_cast<unsigned>(material.route_index),
                static_cast<unsigned>(material.material_slot),
                material.actual_material_exact ? 1u : 0u,
                v220_nonmetal_join ? 1u : 0u,
                local_valid ? 1u : 0u);
            reshade::log::message(reshade::log::level::info, line);
        }
    }
    if (v220_nonmetal_join) {""", 1),
)

def patch(source: str) -> str:
    if "exact_envspec36_or_pmetal_source(" not in source or (
        "sidecar_20261010::exact_nonmetal_material(material)" not in source or
        "if (!exact_pmetal_material_selection(material))" not in source
    ):
        raise RuntimeError("V220 missing V203/V204 exact nonmetal/P_Metal base")
    if source.count(ANCHOR) != 1:
        raise RuntimeError("V220 donor/source anchor drift")
    if source.count("v220_exact_nonmetal_source_key(") > 0:
        # Strict idempotence: inverse everything and compare.
        original = source.replace(HELPER + ANCHOR, ANCHOR, 1)
        for before, after, count in CHANGES:
            if original.count(after) != count:
                raise RuntimeError("V220 patched callsite drift")
            original = original.replace(after, before, count)
        if patch(original) != source:
            raise RuntimeError("V220 generated source drift")
        return source
    updated = source.replace(ANCHOR, HELPER + ANCHOR, 1)
    for before, after, count in CHANGES:
        if updated.count(before) != count or updated.count(after):
            raise RuntimeError("V220 source callsite drift: " + before[:56])
        updated = updated.replace(before, after, count)
    # Never change the R43 P_Metal unkeyed fallback or source selectors.
    for text in (
        "if (!exact_pmetal_material_selection(material))",
        "if (latest_hook_source(out))",
        "exact_envspec36_or_pmetal_source(",
        "source_a_decode_ok",
    ):
        if source.count(text) != updated.count(text):
            raise RuntimeError("V220 P_Metal/source producer mutation detected")
    return updated

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    original = SOURCE.read_text(encoding="utf-8").replace("\r\n", "\n")
    updated = patch(original)
    if args.apply and updated != original:
        SOURCE.write_text(updated, encoding="utf-8", newline="\n")
    elif not args.apply and updated != original:
        raise RuntimeError("V220 overlay not applied")
    print("V220_EXACT_NONMETAL_SOURCE_KEY_" +
          ("APPLIED" if args.apply else "VERIFIED"))
    print("V220_PMETAL_SOURCE_AND_FALLBACK_UNCHANGED=TRUE")
    print("V220_PIXEL_BEHAVIOR=OPEN")

if __name__ == "__main__":
    main()
