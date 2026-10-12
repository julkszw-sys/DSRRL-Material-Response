#!/usr/bin/env python3
"""V228 A/B FULL stock isolation of exact nonmetal HemEnvLerp.

Owner-visible symptom: thin glossy translucent 'laminate' on nonmetal only
during Lerp. V227 source/resource parity proved matching A/B beta and
probe ordinals on P_Metal and nonmetal; did NOT prove material/consumer pixel.
Do not modify P_Metal (working), nonmetal stable HemEnv, or any shader equation.
Route only the complete *exact* nonmetal Lerp dedicated island fail-open to
stock DSR BEFORE generic MR dispatch. Never make diffuse-only hybrids.
Experimental falsifier, NOT a PTDE-fidelity fix.
"""
import argparse
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/"renderer-core/integrated/integrated_addon.cpp"
ANCHOR="""    if (exact_pmetal_envspec) {
        if (!g_pmetal_envspec.prepare("""
REPLACEMENT="""    if (exact_pmetal_envspec) {
#if defined(DSRRL_V228_NONMETAL_LERP_STOCK_ISOLATION)
    // A/B intervention: V227=unchanged full operator, V228=stock only for
    // exactly whitelisted *nonmetal* HemEnvLerp. P_Metal route345 is never
    // eligible. Return before generic MR branch to avoid partial hybrids.
    if (hemenvlerp_bound &&
        material.route_index != 345u) {
        static std::atomic<std::uint32_t> logged{0u};
        if (logged.fetch_add(1u, std::memory_order_relaxed) < 16u) {
            char line[360]{};
            std::snprintf(
                line, sizeof(line),
                "[DSRRL V228 LERP STOCK AB] "
                "rx=%u route=%u slot=%u "
                "whitelisted_nonmetal=1 full_island=STOCK_DSR "
                "generic_mr_fallback=DISALLOWED "
                "pmetal=UNCHANGED stable_nonmetal=UNCHANGED "
                "hypothesis=laminate_is_bridge_composition "
                "pixel=OPEN",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(material.route_index),
                static_cast<unsigned>(material.material_slot));
            reshade::log::message(reshade::log::level::info,line);
        }
        return false;
    }
#endif
        if (!g_pmetal_envspec.prepare("""

def patch(src: str) -> str:
    if src.count(REPLACEMENT) == 1:
        return src
    if src.count(ANCHOR) != 1:
        raise RuntimeError("V228 exact dedicated EnvSpec island drift")
    if src.count("const bool exact_pmetal_envspec =") != 1:
        raise RuntimeError("V228 exact material authority missing")
    if src.count("if (hemenvlerp_bound) {") < 1:
        raise RuntimeError("V228 independent generic MR branch drift")
    if src.index(ANCHOR) > src.index("    if (hemenvlerp_bound) {",src.index(ANCHOR)):
        raise RuntimeError("V228 must intervene before generic Lerp MR")
    return src.replace(ANCHOR,REPLACEMENT,1)

def main():
    a=argparse.ArgumentParser()
    a.add_argument("--apply",action="store_true")
    args=a.parse_args()
    old=SOURCE.read_text(encoding="utf-8").replace("\r\n","\n")
    new=patch(old)
    if args.apply:
        if new!=old:
            SOURCE.write_text(new,encoding="utf-8",newline="\n")
    elif old!=new:
        raise RuntimeError("V228 source is not instrumented")
    print("V228_NONMETAL_LERP_FULL_STOCK_AB_"+("APPLIED" if args.apply else "VERIFIED"))
    print("V228_PMETAL_STABLE_NONMETAL_AND_OTHER_CARRIERS_UNCHANGED")
    print("V228_NOT_PTDE_PIXEL_FIX")

if __name__=="__main__":
    main()
