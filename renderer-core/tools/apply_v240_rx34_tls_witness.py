#!/usr/bin/env python3
"""V240 correct RX33 stable / RX34 Lerp and avoid atomic-RMW on each draw, V237 no-QPC."""
from pathlib import Path
import argparse
PATH=Path("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
OLD="    prepared.ready = true;\n    effect_latch(effect_request_ready_);\n    telemetry::hot_count(requests_);\n"
NEW=OLD+"""
    // V240: bounded RX33 steady / RX34 blend only, actual consumer families.
    // Captures REQUEST_READY, not draw completion or pixel equivalence.
    const bool lerp =
        family == pmetal_envspec_receiver_family::hemenvlerp;
    // The owner runtime identifies Lerp RX34, stable HemEnv RX33.
    if (env_semantics.envspc_slot < 3u &&
        ((lerp && decision.receiver_id == 34u) ||
         (!lerp && decision.receiver_id == 33u))) {
        // Keep hot requests on per-thread cache-line-local bitsets.
        // The global atomic is touched ONLY on a thread's first bin hit.
        static thread_local std::uint32_t local_masks[3][2]{};
        static std::atomic<std::uint32_t> global_masks[3][2]{};
        const unsigned bin =
            !lerp ? 17u :
            (source.beta < 0.0f || source.beta > 1.0f) ? 18u :
            source.beta == 1.0f ? 16u :
            static_cast<unsigned>(source.beta * 16.0f);
        const std::uint32_t flag = 1u << bin;
        auto &local = local_masks[env_semantics.envspc_slot][lerp ? 1 : 0];
        if ((local & flag) == 0u) {
            local |= flag;
            if ((global_masks[env_semantics.envspc_slot][lerp ? 1 : 0]
                     .fetch_or(flag, std::memory_order_relaxed) & flag) == 0u) {
            char line[1100]{};
            std::snprintf(line, sizeof(line),
                "[DSRRL V240 ENDPOINT] stage=REQUEST_READY_NOT_PIXEL "
                "rx=%u route=%u slot=%u class=%u material=%016llx "
                "flver=%016llx family=%s beta=%.9g bin=%u "
                "serial=%llu gen=%llu bankA=%016llx bankB=%016llx "
                "rowA=%u rowB=%u probeA=%u probeB=%u "
                "specA=%.9g,%.9g,%.9g specB=%.9g,%.9g,%.9g "
                "diffA=%.9g,%.9g,%.9g diffB=%.9g,%.9g,%.9g "
                "diffValid=%u c101=%.9g shader=%llx cb=%llx "
                "srvA=%llx srvB=%llx pixel=OPEN",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(decision.route_index),
                static_cast<unsigned>(material.material_slot),
                static_cast<unsigned>(env_semantics.envspc_slot),
                static_cast<unsigned long long>(material.semantic_name_hash),
                static_cast<unsigned long long>(material.flver_identity_hash),
                lerp ? "LERP" : "STABLE",
                static_cast<double>(source.beta), bin,
                static_cast<unsigned long long>(source.serial),
                static_cast<unsigned long long>(source.generation),
                static_cast<unsigned long long>(source.bank_signature_a),
                static_cast<unsigned long long>(source.bank_signature_b),
                static_cast<unsigned>(source.row_id_a),
                static_cast<unsigned>(source.row_id_b),
                static_cast<unsigned>(prepared.env_resources.probe_a),
                static_cast<unsigned>(prepared.env_resources.probe_b),
                static_cast<double>(source.a[0]),
                static_cast<double>(source.a[1]),
                static_cast<double>(source.a[2]),
                static_cast<double>(source.b[0]),
                static_cast<double>(source.b[1]),
                static_cast<double>(source.b[2]),
                static_cast<double>(source.envdiffuse_a[0]),
                static_cast<double>(source.envdiffuse_a[1]),
                static_cast<double>(source.envdiffuse_a[2]),
                static_cast<double>(source.envdiffuse_b[0]),
                static_cast<double>(source.envdiffuse_b[1]),
                static_cast<double>(source.envdiffuse_b[2]),
                source.envdiffuse_linear_valid ? 1u : 0u,
                static_cast<double>(decision.c101),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(prepared.shader)),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(prepared.b12)),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(prepared.env_resources.ptde_a)),
                static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(prepared.env_resources.ptde_b)));
            reshade::log::message(reshade::log::level::info, line);
            }
        }
    }
"""
def patch(s,apply=False):
    stock=s.count(OLD)==1 and NEW not in s
    new=s.count(NEW)==1
    if stock==new: raise ValueError("V240 diagnostic source anchor mismatch")
    for x in ("build_island_draw_mutation(", "source.envdiffuse_a", "prepared.request.srv_count =", "pmetal_envspec_receiver_family::"):
        if x not in s: raise ValueError("V240 semantic guard missing: "+x)
    return s.replace(OLD,NEW,1) if apply and stock else s
if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--apply",action="store_true")
    a=p.parse_args()
    src=PATH.read_text(encoding="utf-8")
    out=patch(src,a.apply)
    if out!=src: PATH.write_text(out,encoding="utf-8")
    print("V240 source contract PASS")
