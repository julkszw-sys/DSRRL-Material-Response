#!/usr/bin/env python3
"""One-addon V258 dormant shader insertion, after authenticated V255 overlays."""
from pathlib import Path
import argparse
P=Path("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
OLD_INC='#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"'
NEW_INC=OLD_INC+'\n#include "dsrrl/runtime/generated_v258_equipment_lerp_dxbc.hpp"'
OLD=(
'''    prepared = {};
    effect_latch(effect_entry_seen_);

    if (cmd_list == nullptr ||''')
NEW=(
'''    prepared = {};
    effect_latch(effect_entry_seen_);

    // V258: actual DXBC compiled into *this* single addon, but NOT
    // authorized as a live DSR receiver replacement. No draw mutation.
    if (family == pmetal_envspec_receiver_family::hemenvlerp &&
        decision.active && material.valid && material.owner_tuple_exact) {
        static std::atomic_bool v258_once{false};
        if (!v258_once.exchange(true,std::memory_order_relaxed)) {
            std::uint32_t witness = 2166136261u;
            for (const auto *raw : {
                    &v258_dxbc::k_env_local_subcut_ps,
                    &v258_dxbc::k_terminal_subcut_ps
                 }) {
                // Never conflate readiness with binding: purposefully
                // evaluate bytes to prove both DXBC blobs are linked.
                const volatile std::uint8_t *bytes = raw->data();
                for (std::size_t i=0u;i<raw->size();++i)
                    witness = (witness ^ bytes[i])*16777619u;
            }
            char info[320]{};
            std::snprintf(info,sizeof(info),
                "[DSRRL V258 PTDE GPU ISLAND DORMANT] "
                "env_dxbc_bytes=%u terminal_dxbc_bytes=%u "
                "linked_witness=%08x receiver=%u "
                "physical_sidecar_authority=OPEN PS_bind=OFF "
                "bridge_active=0 DSR_draw=UNCHANGED PTDE_pixel=OPEN",
                static_cast<unsigned>(
                    v258_dxbc::k_env_local_subcut_ps.size()),
                static_cast<unsigned>(
                    v258_dxbc::k_terminal_subcut_ps.size()),
                static_cast<unsigned>(witness),
                static_cast<unsigned>(decision.receiver_id));
            reshade::log::message(reshade::log::level::info,info);
        }
    }

    if (cmd_list == nullptr ||''')
def one(s,a,b,label):
    if s.count(b)==1:return s
    if s.count(a)!=1:
        raise RuntimeError("V258 "+label+" unexpected source preimage "+str(s.count(a)))
    return s.replace(a,b,1)
def main():
    a=argparse.ArgumentParser();a.add_argument("--apply",action="store_true")
    x=a.parse_args()
    old=P.read_text(encoding="utf8").replace("\r\n","\n")
    new=one(old,OLD_INC,NEW_INC,"include")
    new=one(new,OLD,NEW,"dormant DXBC witness")
    if not x.apply and old!=new:raise RuntimeError("V258 not yet patched")
    if x.apply and old!=new:P.write_text(new,encoding="utf8",newline="\n")
    print("V258_SINGLE_ADDON_DORMANT_DXBC_ISLAND_"+("APPLIED" if x.apply else "PASS"))
if __name__=="__main__":main()
