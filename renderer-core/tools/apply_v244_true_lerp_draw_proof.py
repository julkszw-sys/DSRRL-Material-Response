#!/usr/bin/env python3
"""V244: instrument the REAL bound HemEnvLerp family, not receiver ID as proxy.

One independent read-only observer over the byte-exact V243/V237 no-QPC
runtime. Stage events are bounded to 2 semantic receivers x 5 exact route
buckets x 4 stages (40 per session), not first-hit exhausted by stable PS
draws. Never query the GPU, read QPC, change PS/CB/SRV/sampler, or replay.
"""
from pathlib import Path
import argparse
import re

PATH = Path("renderer-core/integrated/integrated_addon.cpp")
OLD_NAME = "v243_rx34_draw_proof"
NEW_NAME = "v244_true_lerp_draw_proof"
OLD_MARK = "[DSRRL V243 RX DRAW PROOF]"
NEW_MARK = "[DSRRL V244 TRUE LERP DRAW PROOF]"

NEW_FUNCTION = r"""void v244_true_lerp_draw_proof(
    std::uint32_t rx, std::uint32_t route, bool active,
    bool exact_lerp_bound, std::uint32_t slot,
    std::uint64_t semantic_mtd,
    unsigned stage, bool envspec, bool mr, unsigned tx) noexcept
{
    // A numeric MR semantic receiver can identify BOTH stable and Lerp PS.
    // Only the exact pipeline-bound HemEnvLerp identity proves this lane.
    if ((rx != 33u && rx != 34u) || !active ||
        !exact_lerp_bound || stage > 3u)
        return;

    // Route 345 is P_Metal; other routes must never borrow its identity.
    // Keep first-hit masks independently for important nonmetal routes.
    const unsigned route_lane =
        route == 345u ? 0u :
        route == 160u ? 1u :
        route == 1073741869u ? 2u :
        route == 232u ? 3u : 4u;
    const unsigned bit_index =
        (rx - 33u) * 20u + route_lane * 4u + stage;
    const std::uint64_t bit = std::uint64_t{1} << bit_index;
    static thread_local std::uint64_t local_seen = 0u;
    if ((local_seen & bit) != 0u)
        return;
    local_seen |= bit;
    static std::atomic<std::uint64_t> global_seen{0u};
    if ((global_seen.fetch_or(bit, std::memory_order_relaxed) &
         bit) != 0u)
        return;

    const char *name =
        stage == 0u ? "PREPARE_FAIL_STOCK" :
        stage == 1u ? "REQUEST_BATCH_READY" :
        stage == 2u ? "DRAW_ISSUED" : "DISPATCH_FAIL_STOCK";
    char line[420]{};
    std::snprintf(line, sizeof(line),
        "[DSRRL V244 TRUE LERP DRAW PROOF] "
        "family=BOUND_HEMENVLERP rx=%u route=%u class=%s "
        "slot=%u semantic_mtd=%016llx stage=%s "
        "envspec=%u mr=%u tx=%u QPC=OFF pixel=OPEN",
        rx, route, route == 345u ? "PMETAL" : "NONMETAL",
        slot, static_cast<unsigned long long>(semantic_mtd),
        name, envspec ? 1u : 0u, mr ? 1u : 0u, tx);
    reshade::log::message(reshade::log::level::info, line);
}
"""

CALL = re.compile(
    r"v243_rx34_draw_proof\(\s*receiver_id,\s*"
    r"decision\.route_index,\s*decision\.active,\s*"
)
NEW_CALL = (
    "v244_true_lerp_draw_proof(\n"
    "            receiver_id, decision.route_index, decision.active,\n"
    "            hemenvlerp_bound, material.material_slot,\n"
    "            material.semantic_name_hash,\n"
    "            "
)

def patch(s: str) -> str:
    if s.count(NEW_MARK) == 1 and s.count(OLD_NAME) == 0:
        assert s.count(NEW_NAME + "(") == 7
        return s
    if s.count(OLD_MARK) != 1 or s.count(NEW_MARK) != 0:
        raise RuntimeError("V244 missing exact V243 preimage")
    begin = s.find("void " + OLD_NAME + "(")
    if begin < 0:
        raise RuntimeError("V244 V243 function entry missing")
    end = s.find("\n}\n", begin)
    if end < 0:
        raise RuntimeError("V244 V243 function end missing")
    end += len("\n}\n")
    old_function = s[begin:end]
    if old_function.count(OLD_MARK) != 1:
        raise RuntimeError("V244 ambiguous V243 function")
    s = s[:begin] + NEW_FUNCTION + s[end:]
    s, count = CALL.subn(NEW_CALL, s)
    if count != 6:
        raise RuntimeError(
            f"V244 expected 2x prepare/ready/issued, got {count}")
    if s.count(NEW_MARK) != 1 or s.count(NEW_NAME + "(") != 7:
        raise RuntimeError("V244 incomplete exact lane rewrite")
    if s.count(OLD_NAME) != 0 or OLD_MARK in s:
        raise RuntimeError("V244 V243 proxy observer leaked")
    for sentinel in (
        "hemenvlerp_receiver_bound(",
        "dispatch_island_draw_batch(",
        "dispatch_island_draw_indexed_batch(",
        "prepared.envspec_in_batch",
    ):
        if sentinel not in s:
            raise RuntimeError("V244 protected contract missing " + sentinel)
    return s

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    original = PATH.read_text(encoding="utf-8")
    updated = patch(original)
    if args.apply and updated != original:
        PATH.write_text(updated, encoding="utf-8")
    print("V244_EXACT_BOUND_HEMENVLERP_DRAW_PROOF_" +
          ("APPLIED" if args.apply else "VERIFIED" if
           updated == original else "PREIMAGE_READY"))
