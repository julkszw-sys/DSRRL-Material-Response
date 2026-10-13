#!/usr/bin/env python3
"""V252: independent exact NONMETAL true HemEnvLerp draw/CB producer witness.

Run AFTER full V251 temporary generated-source reconstruction.  It does
not make a shader/resource/CB/state or draw decision.  Probe only three
FNV-authenticated MTD routes in the owner's logged mixed-material RX33/34.
"""
from pathlib import Path
import argparse
P = Path("renderer-core/integrated/integrated_addon.cpp")
HELPER_ANCHOR = "bool on_draw(\n    reshade::api::command_list *cmd_list,"
HELPER = r'''// V252 observer, no authorization expansion. MTD FNV identity must agree
// with the pre-existing route and FLVER/slot identity. First stage per
// receiver/MTD only, so no per-draw log allocation or hot-loop D3D calls.
void v252_nonmetal_lerp_stage(
    bool real_lerp,
    std::uint32_t receiver,
    const dsrrl::operators::material_response::material_identity &material,
    const dsrrl::operators::material_response::decision &decision,
    unsigned stage,
    unsigned tx) noexcept
{
    if (!real_lerp || !decision.active ||
        !material.valid || !material.owner_tuple_exact ||
        !material.material_slot_valid ||
        (receiver != 33u && receiver != 34u) || stage > 4u)
        return;

    int route_number = -1;
    if (decision.route_index == 1073741869u &&
        material.route_index == 1073741869u &&
        material.semantic_name_hash == 0xabbcca95a57070c3ull)
        route_number = 0;
    else if (decision.route_index == 160u &&
        material.route_index == 160u &&
        material.semantic_name_hash == 0x73ec84781e69fdc8ull)
        route_number = 1;
    else if (decision.route_index == 232u &&
        material.route_index == 232u &&
        material.semantic_name_hash == 0x0a970c711b27e2cfull)
        route_number = 2;
    if (route_number < 0)
        return;

    const unsigned index =
        (static_cast<unsigned>(route_number) * 2u +
         (receiver == 33u ? 0u : 1u)) * 5u + stage;
    static std::atomic<std::uint64_t> seen{0ull};
    const auto bit = std::uint64_t{1} << index;
    if ((seen.load(std::memory_order_relaxed) & bit) != 0ull ||
        (seen.fetch_or(bit, std::memory_order_relaxed) & bit) != 0ull)
        return;

    const char *name =
        stage == 0u ? "PREPARE_FAIL_STOCK" :
        stage == 1u ? "BATCH_READY" :
        stage == 2u ? "ARMED_NATIVE_UNVERIFIED_ISSUE" :
        stage == 3u ? "DRAW_ISSUED" : "DISPATCH_FAIL_STOCK";
    char log[560]{};
    std::snprintf(log,sizeof(log),
        "[DSRRL V252 NONMETAL LERP] rx=%u route=%u slot=%u "
        "mtd=%016llx stage=%s tx=%u "
        "c100=%.8g,%.8g,%.8g c101=%.8g "
        "ptde_specpower=%.8g verified=%u "
        "owner_tuple=1 actual_flver_slot=1 b12_abi=ENV_PHN_K135 "
        "gpu_cb_content=OPEN pixel=OPEN",
        static_cast<unsigned>(receiver),
        static_cast<unsigned>(decision.route_index),
        static_cast<unsigned>(material.material_slot),
        static_cast<unsigned long long>(material.semantic_name_hash),
        name, tx,
        static_cast<double>(decision.c100[0]),
        static_cast<double>(decision.c100[1]),
        static_cast<double>(decision.c100[2]),
        static_cast<double>(decision.c101),
        static_cast<double>(decision.ptde_specular_power),
        decision.ptde_specular_power_verified ? 1u : 0u);
    reshade::log::message(reshade::log::level::info, log);
}

''' + HELPER_ANCHOR

PREP_FAIL_OLD = '''            receiver_id,
            decision.route_index);
        return false;
    }

    if (prepared.clustered_neutral_noop &&'''
PREP_FAIL_NEW = '''            receiver_id,
            decision.route_index);
        v252_nonmetal_lerp_stage(
            hemenvlerp_bound, receiver_id, material, decision, 0u, 0u);
        return false;
    }

    v252_nonmetal_lerp_stage(
        hemenvlerp_bound, receiver_id, material, decision, 1u, 0u);
    if (prepared.clustered_neutral_noop &&'''

ARM_OLD = '''        release_prepared_island_batch(
            prepared);
        // Return false so ReShade'''
ARM_NEW = '''        v252_nonmetal_lerp_stage(
            hemenvlerp_bound, receiver_id, material, decision, 2u, 0u);
        release_prepared_island_batch(
            prepared);
        // Return false so ReShade'''

DISPATCH_OLD = '''    account_effect_dispatch(
        effect_prepared,
        dispatch.transaction,
        receiver_id,
        decision.route_index);

    if (clustered_in_batch)'''
DISPATCH_NEW = '''    account_effect_dispatch(
        effect_prepared,
        dispatch.transaction,
        receiver_id,
        decision.route_index);

    v252_nonmetal_lerp_stage(
        hemenvlerp_bound, receiver_id, material, decision,
        dsrrl::runtime::draw_tx_issued(dispatch.transaction) ? 3u : 4u,
        static_cast<unsigned>(dispatch.transaction));

    if (clustered_in_batch)'''

def change(text, old, new, count, label):
    if text.count(new) == count:
        return text
    if text.count(old) != count:
        raise RuntimeError(f"V252 {label}: expected {count}, got {text.count(old)}")
    return text.replace(old,new,count)

def patch(text):
    text=change(text,HELPER_ANCHOR,HELPER,1,"helper")
    text=change(text,PREP_FAIL_OLD,PREP_FAIL_NEW,2,"prepare")
    text=change(text,ARM_OLD,ARM_NEW,2,"native arm")
    text=change(text,DISPATCH_OLD,DISPATCH_NEW,2,"dispatch")
    if text.count("[DSRRL V251 LERP NATIVE]") != 0:
        # This stage is independent; the V251 code is in the transaction
        # source rather than integrated_addon.
        raise RuntimeError("V252 unexpectedly inlined V251 transaction logic")
    return text

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--apply", action="store_true")
    args=p.parse_args()
    original=P.read_text(encoding="utf-8").replace("\r\n","\n")
    updated=patch(original)
    if args.apply and updated!=original:
        P.write_text(updated,encoding="utf-8",newline="\n")
    elif not args.apply and updated!=original:
        raise RuntimeError("V252 source patch not applied")
    print("V252_NONMETAL_LERP_EXACT_MTD_STAGE_"+("APPLIED" if args.apply else "PASS"))

if __name__=="__main__":main()
