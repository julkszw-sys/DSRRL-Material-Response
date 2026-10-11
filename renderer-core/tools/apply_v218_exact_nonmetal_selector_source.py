#!/usr/bin/env python3
"""V218 diagnostic source-identity bridge for exactly certified equipment36.

Corrects V203 equipment36's source dead-end without changing the P_Metal
retail source fallback. A nonmetal may publish ONLY its own selector-derived
PTDE source record, keyed to the existing complete material identity and epoch.
No global packer source is permitted as a nonmetal substitute.

V218 does NOT authorize material-family-wide SpecRGB, unknown shader receivers,
or a production pixel claim. It requires the already-existing opt-in V203
equipment36 full owner/slot/raw MTD source/consumer gate.
"""
from pathlib import Path
import argparse

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "renderer-core/src/runtime/pmetal_env_source_runtime.cpp"

def once(s, old, new):
    if new in s:
        if s.count(new) != 1:
            raise RuntimeError("V218 ambiguous installed anchor")
        return s
    if s.count(old) != 1:
        raise RuntimeError(f"V218 source drift: {old[:80]!r}, count={s.count(old)}")
    return s.replace(old, new, 1)

def transform(s):
    s = once(s,
        '#include "dsrrl/runtime/pmetal_selector_policy.hpp"',
        '#include "dsrrl/runtime/pmetal_selector_policy.hpp"\n'
        '#if defined(DSRRL_V218_NONMETAL_SELECTOR_SOURCE_DIAG)\n'
        '#include "dsrrl/runtime/v203_equipment_envspec36_gate.hpp"\n'
        '#endif')

    helper = """
// V218: separate *producer* extension, not P_Metal source impersonation.
// Only the earlier exact 36-owner, full FLVER SHA256+slot+raw MTD semantic
// census is eligible. Do not substitute the broad 1295 texture whitelist.
bool v218_exact_nonmetal_selector_material(
    const operators::material_response::material_identity &material) noexcept
{
#if defined(DSRRL_V218_NONMETAL_SELECTOR_SOURCE_DIAG)
    return v203_equipment::exact_envspec36_material(material);
#else
    (void)material;
    return false;
#endif
}

"""
    anchor = 'bool retail_lightbank_record_index('
    s = once(s, anchor, helper + anchor)

    # Unlike P_Metal, V218 candidates are admitted solely for the already
    # attested retail selector source event; decoding itself is unchanged.
    sel_old = """    if (!g_selector_enabled.load(
            std::memory_order_acquire) ||
        !exact_pmetal_material_selection(
            material) ||
        owner == nullptr ||
        return_address == nullptr)
        return;"""
    sel_new = """    if (!g_selector_enabled.load(
            std::memory_order_acquire) ||
        !(exact_pmetal_material_selection(material) ||
          v218_exact_nonmetal_selector_material(material)) ||
        owner == nullptr ||
        return_address == nullptr)
        return;"""
    s = once(s, sel_old, sel_new)

    # Source retrieval always requires the *same material* and epoch in
    # pmetal_producer_state_latest. Nonmetals MUST NOT consume latest_hook_source,
    # which would implicitly borrow P_Metal/global source identity.
    old = """    if (!g_selector_enabled.load(
            std::memory_order_acquire) ||
        !exact_pmetal_material_selection(
            material)) {
        telemetry::hot_count(
            g_consumer_fail);
        return false;
    }

    if (pmetal_producer_state_latest(
            material,
            epoch,
            out)) {
        telemetry::hot_count(
            g_consumer_ok);
        return true;
    }

    // Narrow fallback carrier"""
    new = """    const bool exact_pmetal =
        exact_pmetal_material_selection(material);
    const bool exact_nonmetal =
        v218_exact_nonmetal_selector_material(material);
    if (!g_selector_enabled.load(
            std::memory_order_acquire) ||
        (!exact_pmetal && !exact_nonmetal)) {
        telemetry::hot_count(
            g_consumer_fail);
        return false;
    }

    if (pmetal_producer_state_latest(
            material,
            epoch,
            out)) {
#if defined(DSRRL_V218_NONMETAL_SELECTOR_SOURCE_DIAG)
        if (exact_nonmetal) {
            static std::atomic<std::uint32_t> hits{0u};
            const auto n=hits.fetch_add(1u,std::memory_order_relaxed)+1u;
            if (n<=12u || (n<=4096u && (n&(n-1u))==0u))
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL V218 SELECTOR SOURCE] stage=nonmetal_exact_owner "
                    "full_flver_slot_mtd=1 epoch_match=1 global_fallback=0 "
                    "consumer_not_authorized=1 pixel=OPEN");
        }
#endif
        telemetry::hot_count(g_consumer_ok);
        return true;
    }

    // A P_Metal hook source has no certified nonmetal owner relationship:
    // never borrow it when the exact nonmetal selector record is unavailable.
    if (exact_nonmetal) {
        telemetry::hot_count(g_consumer_fail);
        return false;
    }

    // Narrow fallback carrier"""
    s = once(s, old, new)
    return s

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--apply", action="store_true")
    args = p.parse_args()
    text = SOURCE.read_text(encoding="utf-8").replace("\r\n", "\n")
    patched = transform(text)
    if args.apply and patched != text:
        SOURCE.write_text(patched, encoding="utf-8", newline="\n")
    elif not args.apply and patched != text:
        raise RuntimeError("V218 source producer overlay missing")
    assert patched.count("v218_exact_nonmetal_selector_material") == 3
    assert "if (exact_nonmetal) {\n        telemetry::hot_count(g_consumer_fail);\n        return false;\n    }" in patched
    assert patched.index("if (exact_nonmetal) {\n        telemetry::hot_count(g_consumer_fail);") < patched.index("if (latest_hook_source(out))")
    print("V218_EXACT_NONMETAL_SELECTOR_SOURCE_" +
          ("APPLIED" if args.apply else "VERIFIED"))
    print("V218_CERTIFIED_SOURCE=EXACT_36_OWNER_SELECTOR_EPOCH_ONLY")
    print("V218_NONMETAL_GLOBAL_HOOK_FALLBACK=FORBIDDEN")

if __name__ == "__main__":
    main()
