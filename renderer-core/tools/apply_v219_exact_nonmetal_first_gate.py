#!/usr/bin/env python3
"""V219 first failing *actual draw* material operator cut.
Applies only after existing V203/V204/V216 generated overlays.
No changes to pixel shader, producer, CB, SRV mutation, or authorizations.
This is a bounded owner-triggered exception to RELEASE_CLEANUP logging: the
owner's 30-second equip-switch run should be sufficient to show the gate.
"""
from pathlib import Path
import argparse
ROOT=Path(__file__).resolve().parents[2]
INTEGRATED=ROOT/"renderer-core/integrated/integrated_addon.cpp"
ENVSPEC=ROOT/"renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp"

def once(s,a,b):
    if s.count(b)==1:return s
    if s.count(a)!=1:raise RuntimeError(f"V219 code drift {a[:90]!r}, count={s.count(a)}")
    return s.replace(a,b,1)

def patch_integrated(s):
    old="""    const bool exact_pmetal_envspec =
        dsrrl::runtime::exact_pmetal_envspec_candidate(
            material,
            decision);

    if (exact_pmetal_envspec) {"""
    new="""    const bool exact_pmetal_envspec =
        dsrrl::runtime::exact_pmetal_envspec_candidate(
            material,
            decision);

#if defined(DSRRL_V219_EXACT_NONMETAL_GATE_TRACE)
    // First record for each exact FLVER+slot+MTD/receiver *draw*; not the
    // first 16 startup texture creation callbacks. These hashes dedupe
    // telemetry only and confer NO material/consumer authorization.
    if (material.valid && material.owner_tuple_exact &&
        material.material_slot_valid && material.route_index!=345u &&
        material.material_family_hash ==
            dsrrl::operators::material_response::
                mtd_semantic_hash("DifSpcBmp") &&
        decision.receiver_id>=33u && decision.receiver_id<=35u) {
        static std::mutex mutex;
        static std::array<std::uint64_t,96u> fingerprints{};
        static std::size_t used=0u;
        std::uint64_t key=material.flver_identity_hash;
        key^=(static_cast<std::uint64_t>(material.material_slot)<<37u);
        key^=(static_cast<std::uint64_t>(material.route_index)<<18u);
        key^=(static_cast<std::uint64_t>(decision.receiver_id)<<9u);
        // Material semantic identity is included to distinguish mixed slots.
        key^=material.semantic_name_hash;
        bool report=false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (std::find(fingerprints.begin(),
                          fingerprints.begin()+used,key)==
                    fingerprints.begin()+used && used<fingerprints.size()) {
                fingerprints[used++]=key;
                report=true;
            }
        }
        if (report) {
            char line[512]{};
            std::snprintf(line,sizeof(line),
                "[DSRRL V219 MATERIAL DRAW] "
                "rx=%u route=%u flver=%016llx slot=%u "
                "mtd=%016llx owner=1 decision=%u "
                "dedicated_envspec_candidate=%u "
                "if_not_candidate=stock_DSR",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(material.route_index),
                static_cast<unsigned long long>(material.flver_identity_hash),
                static_cast<unsigned>(material.material_slot),
                static_cast<unsigned long long>(material.semantic_name_hash),
                decision.active?1u:0u,
                exact_pmetal_envspec?1u:0u);
            reshade::log::message(reshade::log::level::info,line);
        }
    }
#endif

    if (exact_pmetal_envspec) {"""
    return once(s,old,new)

def patch_envspec(s):
    # Existing granular stage logging (material, source, probe, spec,
    # mutation) is compiled out under RELEASE_CLEANUP. Restore it for ONLY
    # this explicit diagnostic; never change a render decision.
    s=once(s,
        '#if !defined(DSRRL_RELEASE_CLEANUP)\nstd::atomic<std::uint32_t> g_prepare_stage_log_mask{0u};',
        '#if !defined(DSRRL_RELEASE_CLEANUP) || defined(DSRRL_V219_EXACT_NONMETAL_GATE_TRACE)\nstd::atomic<std::uint32_t> g_prepare_stage_log_mask{0u};')
    old="""#if defined(DSRRL_RELEASE_CLEANUP)
    (void)bit;
    (void)stage;
    (void)material;
    (void)decision;
    (void)family;
#else
    const auto observed ="""
    new="""#if defined(DSRRL_RELEASE_CLEANUP) && !defined(DSRRL_V219_EXACT_NONMETAL_GATE_TRACE)
    (void)bit;
    (void)stage;
    (void)material;
    (void)decision;
    (void)family;
#else
#if defined(DSRRL_V219_EXACT_NONMETAL_GATE_TRACE)
    // Keep original P_Metal clean and do not spam world/unknown receivers.
    if (!material.valid || !material.owner_tuple_exact ||
        !material.material_slot_valid || material.route_index==345u)
        return;
#endif
    const auto observed ="""
    s=once(s,old,new)
    return s

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--apply",action="store_true")
    args=p.parse_args()
    for path,fn in ((INTEGRATED,patch_integrated),(ENVSPEC,patch_envspec)):
        original=path.read_text(encoding="utf-8").replace("\r\n","\n")
        updated=fn(original)
        if args.apply and updated!=original:
            path.write_text(updated,encoding="utf-8",newline="\n")
        elif not args.apply and updated!=original:
            raise RuntimeError("V219 owner-triggered operator source was not compiled: "+str(path))
    print("V219_OWNER_FIRST_GATE_"+("APPLIED" if args.apply else "VERIFIED"))
    print("V219_RENDER_SEMANTICS_UNCHANGED=ON")

if __name__=="__main__":
    main()
