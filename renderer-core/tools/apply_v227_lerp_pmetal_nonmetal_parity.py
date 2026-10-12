#!/usr/bin/env python3
"""V227 bounded side-by-side P_Metal/nonmetal Lerp source and resource proof.

V223 shared LightBank is known to work on original P_Metal HemEnvLerp.
The user reports nonmetal Lerp divergence, so DO NOT mutate the successful
source path or force keyed-only source. Compare the actual material/receiver,
A/B source, beta, bound stock A/B resource identity and exact PTDE probe
readiness on the same draw instead. This is diagnostic, not a solver.
"""
import argparse
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
DRAW=ROOT/"renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp"

ANCHOR="""    if (!env_resource_ready) {
        if (shader != nullptr)
            shader->Release();
        telemetry::hot_count(probe_rejects_);"""
INSERT="""#if defined(DSRRL_V227_LERP_PM_NONMETAL_PARITY)
    // Strictly bounded Lerp comparison at the same post-V224 probe cut.
    // The full P_Metal and nonmetal operator paths below are unchanged.
    if (family == pmetal_envspec_receiver_family::hemenvlerp) {
        const bool v227_pmetal = exact_pmetal_material(material);
        const bool v227_nonmetal = !v227_pmetal &&
            sidecar_20261010::exact_nonmetal_draw(material, decision);
        if (v227_pmetal || v227_nonmetal) {
            static std::atomic<std::uint32_t> pmetal_seen{0u};
            static std::atomic<std::uint32_t> nonmetal_seen{0u};
            auto &seen = v227_pmetal ? pmetal_seen : nonmetal_seen;
            auto count = seen.load(std::memory_order_relaxed);
            if (count < 8u &&
                seen.compare_exchange_strong(
                    count, count + 1u, std::memory_order_relaxed)) {
                // D3D11 queries occur on at most 8 representative draws
                // per class, not on the production draw hot path.
                ID3D11ShaderResourceView *live[3]{};
                context->PSGetShaderResources(12u, 3u, live);
                ID3D11Resource *res_a = nullptr;
                ID3D11Resource *res_b = nullptr;
                if (live[0] != nullptr) live[0]->GetResource(&res_a);
                if (live[2] != nullptr) live[2]->GetResource(&res_b);
                const auto native_a = reinterpret_cast<std::uintptr_t>(live[0]);
                const auto native_b = reinterpret_cast<std::uintptr_t>(live[2]);
                const auto resource_a = reinterpret_cast<std::uintptr_t>(res_a);
                const auto resource_b = reinterpret_cast<std::uintptr_t>(res_b);
                if (res_a != nullptr) res_a->Release();
                if (res_b != nullptr) res_b->Release();
                for (auto *view : live) if (view != nullptr) view->Release();

                char line[1100]{};
                std::snprintf(line, sizeof(line),
                    "[DSRRL V227 LERP PARITY] "
                    "class=%s rx=%u route=%u slot=%u env_slot=%u "
                    "mtd=%016llx flver=%016llx "
                    "beta=%.9g src_serial=%llu src_generation=%llu "
                    "bank_a=%016llx row_a=%u bank_b=%016llx row_b=%u "
                    "envdiffuse_linear=%u ab_required=%u shader_ready=%u "
                    "native_a=%llx native_b=%llx "
                    "resource_a=%llx resource_b=%llx "
                    "env_probe_ready=%u ptde_probe_a=%u ptde_probe_b=%u "
                    "shadow_ready=%u "
                    "source_unchanged=1 bridge_unchanged=1 pixel=OPEN",
                    v227_pmetal ? "PMETAL" : "NONMETAL",
                    static_cast<unsigned>(decision.receiver_id),
                    static_cast<unsigned>(material.route_index),
                    static_cast<unsigned>(material.material_slot),
                    static_cast<unsigned>(env_semantics.envspc_slot),
                    static_cast<unsigned long long>(material.semantic_name_hash),
                    static_cast<unsigned long long>(material.flver_identity_hash),
                    static_cast<double>(source.beta),
                    static_cast<unsigned long long>(source.serial),
                    static_cast<unsigned long long>(source.generation),
                    static_cast<unsigned long long>(source.bank_signature_a),
                    static_cast<unsigned>(source.row_id_a),
                    static_cast<unsigned long long>(source.bank_signature_b),
                    static_cast<unsigned>(source.row_id_b),
                    source.envdiffuse_linear_valid ? 1u : 0u,
                    probe_b_required ? 1u : 0u,
                    shader != nullptr ? 1u : 0u,
                    static_cast<unsigned long long>(native_a),
                    static_cast<unsigned long long>(native_b),
                    static_cast<unsigned long long>(resource_a),
                    static_cast<unsigned long long>(resource_b),
                    env_resource_ready ? 1u : 0u,
                    static_cast<unsigned>(prepared.env_resources.probe_a),
                    static_cast<unsigned>(prepared.env_resources.probe_b),
                    shadow_env_ready ? 1u : 0u);
                reshade::log::message(reshade::log::level::info, line);
            }
        }
    }
#endif

""" + ANCHOR

def patch(src:str)->str:
    if src.count(INSERT)==1:
        return src
    if src.count(ANCHOR)!=1:
        raise RuntimeError("V227 post-V224 probe fail-open anchor drift")
    for required in (
       "prepare_exact_native_resource_join_v224",
       "[DSRRL V224 NATIVE RESOURCE JOIN]",
       "[DSRRL V223 SHARED LIGHTBANK]" # in separate source, not draw: exclude
    )[:2]:
        if required not in src:
            raise RuntimeError("V227 requires V224 prepared draw path")
    if "exact_pmetal_material(material)" not in src:
        raise RuntimeError("P_Metal identity guard drift")
    return src.replace(ANCHOR, INSERT,1)

def main():
    a=argparse.ArgumentParser()
    a.add_argument("--apply",action="store_true")
    args=a.parse_args()
    original=DRAW.read_text(encoding="utf-8").replace("\r\n","\n")
    updated=patch(original)
    if args.apply:
        if updated!=original:
            DRAW.write_text(updated,encoding="utf-8",newline="\n")
    elif updated!=original:
        raise RuntimeError("V227 bounded parity witness not materialized")
    print("V227_LERP_PMETAL_NONMETAL_PARITY_"+("APPLIED" if args.apply else "VERIFIED"))
    print("V227_SOURCE_SHADER_BRIDGE_OUTCOMES_UNCHANGED=1")
    print("V227_RUNTIME_AND_PIXEL_OPEN")

if __name__=="__main__":
    main()
