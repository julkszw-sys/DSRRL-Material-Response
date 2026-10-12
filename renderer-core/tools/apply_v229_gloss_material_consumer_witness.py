#!/usr/bin/env python3
"""V229 exact material/consumer witness; does NOT change any renderer equation.

Owner: some authenticated nonmetals look too shiny even outside HemEnvLerp,
and during Lerp show a translucent gloss laminate. V227 confirms shared
LightBank beta and probe ordinal but NOT same PTDE material cubemap slot,
SpecRGB c101, t10 companion, normals, roughness or draw activation.

Log one successful prepare for each authenticated material+slot+receiver+
family combination (24 bounded per class), both STABLE and LERP on original
P_Metal vs authenticated nonmetal. Keep source, shader/CB/SRV outputs,
operator and native draw behavior byte-for-byte unchanged.
"""
import argparse
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
DRAW=ROOT/"renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp"
ANCHOR="""    prepared.ready = true;
    effect_latch(effect_request_ready_);
    telemetry::hot_count(requests_);"""
INSERT="""    prepared.ready = true;
    effect_latch(effect_request_ready_);
    telemetry::hot_count(requests_);
#if defined(DSRRL_V229_GLOSS_MATERIAL_CONSUMER_WITNESS)
    // Successful *prepared* shader/CB/SRV cut. Native draw confirmation
    // and PTDE pixel response are separate stages; neither is inferred here.
    const bool v229_pmetal = exact_pmetal_material(material);
    const bool v229_lerp =
        family == pmetal_envspec_receiver_family::hemenvlerp;
    const std::uint32_t v229_class =
        (v229_pmetal ? 0u : 2u) + (v229_lerp ? 1u : 0u);
    static std::array<
        std::array<std::atomic<std::uint64_t>,24>,4> v229_seen{};
    const std::uint64_t v229_key =
        (static_cast<std::uint64_t>(material.route_index) << 24u) |
        (static_cast<std::uint64_t>(material.material_slot) << 8u) |
        static_cast<std::uint64_t>(decision.receiver_id);
    bool v229_unique = false;
    for (auto &entry : v229_seen[v229_class]) {
        std::uint64_t observed =
            entry.load(std::memory_order_relaxed);
        if (observed == v229_key)
            break;
        if (observed == 0u &&
            entry.compare_exchange_strong(
                observed,
                v229_key,
                std::memory_order_relaxed)) {
            v229_unique = true;
            break;
        }
    }
    if (v229_unique) {
        ID3D11ShaderResourceView *v229_stock_t1 = nullptr;
        context->PSGetShaderResources(
            1u, 1u, &v229_stock_t1);
        D3D11_SHADER_RESOURCE_VIEW_DESC v229_stock_desc{};
        if (v229_stock_t1 != nullptr)
            v229_stock_t1->GetDesc(&v229_stock_desc);

        ID3D11ShaderResourceView *v229_t10 = nullptr;
        std::uint32_t v229_t10_count = 0u;
        std::uint32_t v229_t0_count = 0u;
        std::uint32_t v229_t2_count = 0u;
        for (std::uint32_t r = 0u;
             r < prepared.material_resources.request_count; ++r) {
            const auto &req = prepared.material_resources.requests[r];
            for (std::uint32_t j = 0u; j < req.srv_count; ++j) {
                if (req.srvs[j].slot == 10u &&
                    req.srvs[j].srv != nullptr) {
                    v229_t10 = req.srvs[j].srv;
                    ++v229_t10_count;
                }
                if (req.srvs[j].slot == 0u &&
                    req.srvs[j].srv != nullptr)
                    ++v229_t0_count;
                if (req.srvs[j].slot == 2u &&
                    req.srvs[j].srv != nullptr)
                    ++v229_t2_count;
            }
        }
        D3D11_SHADER_RESOURCE_VIEW_DESC v229_t10_desc{};
        if (v229_t10 != nullptr)
            v229_t10->GetDesc(&v229_t10_desc);

        char line[1850]{};
        std::snprintf(
            line, sizeof(line),
            "[DSRRL V229 GLOSS CUT] "
            "class=%s family=%s rx=%u route=%u slot=%u "
            "mtd=%016llx env_slot=%u probe=%u,%u beta=%.9g "
            "c101=%.9g ptde_specpower=%.9g power_verified=%u "
            "c100=%.9g,%.9g,%.9g "
            "t1=%llx t1_dim=%u t1_fmt=%u "
            "t10=%llx t10_count=%u t10_dim=%u t10_fmt=%u "
            "spec=%u diffuse=%u normal=%u req_count=%u "
            "t0_count=%u t2_count=%u "
            "envspec_cubeA=%llx cubeB=%llx "
            "envdiffA=%llx envdiffB=%llx envdifflinear=%u "
            "shader=%llx b12=%llx "
            "stage=PREPARED_ONLY pixel=OPEN",
            v229_pmetal ? "PMETAL" : "NONMETAL",
            v229_lerp ? "LERP" : "STABLE",
            static_cast<unsigned>(decision.receiver_id),
            static_cast<unsigned>(material.route_index),
            static_cast<unsigned>(material.material_slot),
            static_cast<unsigned long long>(material.semantic_name_hash),
            static_cast<unsigned>(env_semantics.envspc_slot),
            static_cast<unsigned>(prepared.env_resources.probe_a),
            static_cast<unsigned>(prepared.env_resources.probe_b),
            static_cast<double>(source.beta),
            static_cast<double>(decision.c101),
            static_cast<double>(decision.ptde_specular_power),
            decision.ptde_specular_power_verified ? 1u : 0u,
            static_cast<double>(decision.c100[0]),
            static_cast<double>(decision.c100[1]),
            static_cast<double>(decision.c100[2]),
            static_cast<unsigned long long>(
                reinterpret_cast<std::uintptr_t>(v229_stock_t1)),
            static_cast<unsigned>(v229_stock_desc.ViewDimension),
            static_cast<unsigned>(v229_stock_desc.Format),
            static_cast<unsigned long long>(
                reinterpret_cast<std::uintptr_t>(v229_t10)),
            static_cast<unsigned>(v229_t10_count),
            static_cast<unsigned>(v229_t10_desc.ViewDimension),
            static_cast<unsigned>(v229_t10_desc.Format),
            prepared.material_resources.spec_rgb ? 1u : 0u,
            prepared.material_resources.diffuse ? 1u : 0u,
            prepared.material_resources.normal ? 1u : 0u,
            static_cast<unsigned>(
                prepared.material_resources.request_count),
            static_cast<unsigned>(v229_t0_count),
            static_cast<unsigned>(v229_t2_count),
            static_cast<unsigned long long>(
                reinterpret_cast<std::uintptr_t>(
                    prepared.env_resources.ptde_a)),
            static_cast<unsigned long long>(
                reinterpret_cast<std::uintptr_t>(
                    prepared.env_resources.ptde_b)),
            static_cast<unsigned long long>(
                reinterpret_cast<std::uintptr_t>(
                    prepared.envdiffuse_resources.ptde_a)),
            static_cast<unsigned long long>(
                reinterpret_cast<std::uintptr_t>(
                    prepared.envdiffuse_resources.ptde_b)),
            source.envdiffuse_linear_valid ? 1u : 0u,
            static_cast<unsigned long long>(
                reinterpret_cast<std::uintptr_t>(prepared.shader)),
            static_cast<unsigned long long>(
                reinterpret_cast<std::uintptr_t>(prepared.b12)));
        reshade::log::message(
            reshade::log::level::info, line);

        if (v229_stock_t1 != nullptr)
            v229_stock_t1->Release();
    }
#endif"""
def patch(s: str) -> str:
    if s.count(INSERT) == 1:
        return s
    if s.count(ANCHOR) != 1:
        raise ValueError("V229 post-consumer ready anchor not unique")
    if "exact_pmetal_material(material)" not in s or (
        "prepare_exact_native_resource_join_v224" not in s):
        raise ValueError("V229 requires native V224 and P_Metal material gates")
    if "if (family ==" not in s:
        raise ValueError("V229 Lerp family guard absent")
    return s.replace(ANCHOR, INSERT, 1)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--apply",action="store_true")
    args=ap.parse_args()
    old=DRAW.read_text(encoding="utf-8").replace("\r\n","\n")
    new=patch(old)
    if args.apply:
        if new!=old:
            DRAW.write_text(new,encoding="utf-8",newline="\n")
    elif new!=old:
        raise RuntimeError("V229 shader/material witness not materialized")
    print("V229_GLOSS_MATERIAL_CONSUMER_WITNESS_"+("APPLIED" if args.apply else "VERIFIED"))
    print("V229_SHARED_LIGHTBANK_PTDE_PROBES_SHADER_MATERIAL_REQUESTS_UNCHANGED")
    print("V229_RENDERER_BEHAVIOR_UNCHANGED_PIXEL_OPEN")

if __name__=="__main__":
    main()
