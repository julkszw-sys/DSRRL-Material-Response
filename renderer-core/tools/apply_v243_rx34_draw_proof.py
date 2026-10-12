#!/usr/bin/env python3
"""V243: prove whether RX34 Lerp reaches issued modified Draw, not just PREPARED.

Build-only temporary overlay on V237's source exact output. Only first
occurrence of each receiver/material/stage is logged (12-bit mask) with
TLS-first guard. No shaders, CB, SRV, source, producer, routing or frame
timing changed. RX33 steady baseline and RX34 Lerp separately observed.
"""
from pathlib import Path
import argparse
PATH=Path("renderer-core/integrated/integrated_addon.cpp")
GLOBAL="std::atomic<std::uint64_t> g_lerp_full_draw_fallback{0};\n"
GLOBAL_ADD=GLOBAL+"""
// V243: bounded observable PREPARED != DRAW_ISSUED check for exact RX33/34.
// No QPC, no producer locks. Per-thread first-event filter prevents a
// shared atomic RMW on the ordinary hot path.
void v243_rx34_draw_proof(
    std::uint32_t rx, std::uint32_t route, bool active,
    unsigned stage, bool envspec, bool mr, unsigned tx) noexcept
{
    if ((rx != 33u && rx != 34u) || !active || stage > 3u)
        return;
    const unsigned cls = route == 345u ? 0u : 1u;
    const unsigned bit_index = (rx - 33u) * 8u + cls * 4u + stage;
    const std::uint32_t bit = 1u << bit_index;
    static thread_local std::uint32_t local_seen = 0u;
    if ((local_seen & bit) != 0u)
        return;
    local_seen |= bit;
    static std::atomic<std::uint32_t> global_seen{0u};
    if ((global_seen.fetch_or(bit, std::memory_order_relaxed) & bit) != 0u)
        return;
    const char *name =
        stage == 0u ? "PREPARE_FAIL_STOCK" :
        stage == 1u ? "REQUEST_BATCH_READY" :
        stage == 2u ? "DRAW_ISSUED" : "DISPATCH_FAIL_STOCK";
    char line[280]{};
    std::snprintf(line, sizeof(line),
        "[DSRRL V243 RX DRAW PROOF] rx=%u route=%u class=%s "
        "stage=%s envspec=%u mr=%u tx=%u "
        "QPC=OFF pixel=OPEN",
        rx, route, cls == 0u ? "PMETAL" : "NONMETAL",
        name, envspec ? 1u : 0u, mr ? 1u : 0u, tx);
    reshade::log::message(reshade::log::level::info, line);
}
"""
FAIL="""        mark_effect_probe_mask(
            effect_candidates,
            effect_probe_stage::fail_open,
            receiver_id,
            decision.route_index);
        return false;
    }
"""
FAIL_ADD="""        v243_rx34_draw_proof(
            receiver_id, decision.route_index, decision.active,
            0u, false, false, 0u);
"""+FAIL
READY="""    const auto effect_prepared =
        prepared_effect_mask(prepared);
"""
READY_ADD=READY+"""    v243_rx34_draw_proof(
        receiver_id, decision.route_index, decision.active,
        1u, prepared.envspec_in_batch,
        prepared.mr_in_batch, 0u);
"""
ISSUE="""    account_effect_dispatch(
        effect_prepared,
        dispatch.transaction,
        receiver_id,
        decision.route_index);
"""
ISSUE_ADD=ISSUE+"""    v243_rx34_draw_proof(
        receiver_id, decision.route_index, decision.active,
        dsrrl::runtime::draw_tx_issued(dispatch.transaction) ? 2u : 3u,
        (effect_prepared &
         effect_probe_bit(effect_probe_id::pmetal_envspec)) != 0u,
        mr_in_batch,
        static_cast<unsigned>(dispatch.transaction));
"""
def patch(s, apply=False):
    orig=(s.count(GLOBAL)==1 and GLOBAL_ADD not in s and
          s.count(FAIL)==2 and s.count(READY)==2 and s.count(ISSUE)==2 and
          ISSUE_ADD not in s)
    new=(s.count(GLOBAL_ADD)==1 and
         s.count(FAIL_ADD)==2 and s.count(READY_ADD)==2 and
         s.count(ISSUE_ADD)==2)
    if orig==new: raise ValueError("V243 source/probe anchors mismatch")
    for x in ("k_pmetal_native_draw_runtime_enabled = false",
              "k_pmetal_direct_current_native_dispatch = true",
              "if (!prepare_island_batch(", "dispatch_island_draw_batch(",
              "dispatch_island_draw_indexed_batch(", "prepared.envspec_in_batch"):
        if x not in s:raise ValueError("V243 protected contract absent: "+x)
    if not apply or new:return s
    return (s.replace(GLOBAL,GLOBAL_ADD,1)
             .replace(FAIL,FAIL_ADD,2)
             .replace(READY,READY_ADD,2)
             .replace(ISSUE,ISSUE_ADD,2))
if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--apply",action="store_true")
    args=parser.parse_args()
    old=PATH.read_text(encoding="utf-8")
    new=patch(old,args.apply)
    if new!=old:PATH.write_text(new,encoding="utf-8")
    print("V243_RX34_DRAW_ISSUE_AND_FAIL_OPEN_SOURCE_ORACLE_PASS")
