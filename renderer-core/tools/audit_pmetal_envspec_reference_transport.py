#!/usr/bin/env python3
import argparse
from pathlib import Path


def fail(msg: str) -> None:
    raise RuntimeError(msg)


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        fail(f"{label}: missing {needle!r}")


def block_between(text: str, begin_needle: str, end_needle: str, label: str) -> str:
    begin = text.find(begin_needle)
    if begin < 0:
        fail(f"{label}: begin not found")
    end = text.find(end_needle, begin + len(begin_needle))
    if end <= begin:
        fail(f"{label}: end not found")
    return text[begin:end]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source-dir", required=True)
    ns = ap.parse_args()
    root = Path(ns.source_dir).resolve()

    integrated = (root / "integrated/integrated_addon.cpp").read_text(encoding="utf-8")
    ul_h = (root / "include/dsrrl/runtime/upper_lower_draw_runtime.hpp").read_text(encoding="utf-8")
    ul_cpp = (root / "src/runtime/upper_lower_draw_runtime.cpp").read_text(encoding="utf-8")
    env_cpp = (root / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
    flver_cpp = (root / "src/runtime/flver_engine_hooks.cpp").read_text(encoding="utf-8")
    lerp_cpp = (root / "src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text(encoding="utf-8")

    # Shared LightBank routing stays independent of visible U/L.
    require(integrated, "const bool pmetal_envspec_enabled =", "P_Metal EnvSpec feature gate")
    require(
        integrated,
        "const bool lightbank_reference_transport_required =\n        upper_lower_enabled ||\n        pmetal_envspec_enabled;",
        "LightBank transport independent of U/L",
    )
    require(
        integrated,
        "g_upper_lower.install(\n             !upper_lower_enabled)",
        "reference-only install when U/L is disabled",
    )
    require(
        ul_h,
        "bool install(bool reference_only = false) noexcept;",
        "reference-only API",
    )
    require(ul_cpp, "g_reference_only_transport", "reference-only runtime flag")

    # The rejected dedicated 0x563C30 startup hook must remain unarmed.
    if "!install_optional_pmetal_env_hook()" in ul_cpp:
        fail("dedicated 0x563C30 P_Metal source hook is armed again")
    require(
        ul_cpp,
        "g_pmetal_env_hook_armed.store(false);",
        "standalone P_Metal source hook disabled",
    )

    # Producer hot path may carry only source tuple identity. It must not hash
    # LightBank banks or decode PTDE P_Metal donor values globally.
    steady = block_between(
        ul_cpp,
        "void __fastcall hook_steady_packer(",
        "\nvoid *__fastcall hook_blend(",
        "steady packer",
    )
    steady_ref = block_between(
        steady,
        "if (g_reference_only_transport.load(",
        "\n            if (capture_evaluated_vectors(",
        "steady reference-only block",
    )
    require(
        steady_ref,
        "producer-cheap",
        "steady producer-hot-path policy",
    )
    if "read_exact_pmetal_env_source(" in steady_ref or "pmetal_bank_signature(" in steady_ref:
        fail("steady reference-only producer still decodes/hashes P_Metal source")

    blend = block_between(
        ul_cpp,
        "void *__fastcall hook_blend_packer(",
        "\nvoid __fastcall hook_pmetal_env_blend(",
        "blend packer",
    )
    blend_ref = block_between(
        blend,
        "if (g_reference_only_transport.load(",
        "\n            if (capture_evaluated_vectors(",
        "blend reference-only block",
    )
    if "read_exact_pmetal_env_source(" in blend_ref or "pmetal_bank_signature(" in blend_ref:
        fail("blend reference-only producer still decodes/hashes P_Metal source")

    # Exact producer tuple is still published and then authorized only by the
    # exact selector + exact actual P_Metal material join.
    require(
        ul_cpp,
        "token.source_a = producer.source_a;",
        "source A tuple publication",
    )
    require(
        ul_cpp,
        "token.source_b = producer.source_b;",
        "source B tuple publication",
    )
    require(
        ul_cpp,
        "token.selector_a = producer.selector_a;",
        "selector A tuple publication",
    )
    require(
        ul_cpp,
        "token.selector_b = producer.selector_b;",
        "selector B tuple publication",
    )
    require(
        ul_cpp,
        "producer.pmetal_bank_base_a = bank_base_a;",
        "engine-live stable bank A capture",
    )
    require(
        ul_cpp,
        "producer.pmetal_bank_base_b = bank_base_b;",
        "engine-live stable bank B capture",
    )
    require(
        ul_cpp,
        "token.pmetal_bank_base_a =",
        "stable bank A token publication",
    )
    require(
        ul_cpp,
        "token.pmetal_bank_base_b =",
        "stable bank B token publication",
    )
    require(
        flver_cpp,
        "upper_lower_pmetal_material_event_bridge(",
        "FLVER selector material authorization bridge",
    )

    pmetal_event = block_between(
        ul_cpp,
        "void upper_lower_draw_runtime::pmetal_material_event(",
        "\nbool upper_lower_draw_runtime::direct_producer_active()",
        "P_Metal material event",
    )
    require(
        pmetal_event,
        "exact_pmetal_material_selection(",
        "exact P_Metal material gate",
    )
    require(
        pmetal_event,
        "invalidate_selected_reference_token_for_producer(",
        "stale selected-state invalidation",
    )
    require(
        pmetal_event,
        "publish_selected_reference_token(\n        g_draw_reference_token);",
        "exact source-tuple publication",
    )
    if "read_exact_pmetal_env_source(" in pmetal_event:
        fail("P_Metal material callback performs delayed bank decode")
    if "materialize_selected_pmetal_env_source(" in ul_cpp:
        fail("obsolete producer-materialized P_Metal donor helper still present")

    # Consumer-local source decode must happen only after exact P_Metal material
    # and exact EnvSpec semantic gates in pmetal_envspec_draw_runtime.
    material_gate = env_cpp.find("if (!exact_pmetal_material(")
    semantic_gate = env_cpp.find("if (!env_semantics.exact_identity_match")
    source_call = env_cpp.find("selected_pmetal_env_source(")
    if material_gate < 0 or semantic_gate < 0 or source_call < 0:
        fail("cannot establish P_Metal consumer gate ordering")
    if not (material_gate < semantic_gate < source_call):
        fail("P_Metal source decode is not downstream of material+semantic gates")

    selected = block_between(
        ul_cpp,
        "bool upper_lower_draw_runtime::selected_pmetal_env_source(",
        "\npmetal_env_source_diagnostic",
        "selected P_Metal source",
    )
    require(selected, "!token.source_ready", "selected source readiness gate")
    require(
        selected,
        "read_exact_pmetal_env_base(\n                token.pmetal_bank_base_a,",
        "lazy exact bank A decode",
    )
    require(
        selected,
        "read_exact_pmetal_env_base(\n                    token.pmetal_bank_base_b,",
        "lazy exact bank B decode",
    )
    if "token.source_a" in selected or "token.source_b" in selected:
        fail("selected P_Metal source consumer dereferences producer source objects")
    require(
        selected,
        "Consumer-local lazy decode",
        "consumer-local lazy decode policy",
    )

    # Every delayed engine pointer dereference must fail open through safe_read.
    resolver = block_between(
        ul_cpp,
        "bool read_exact_pmetal_env_base(",
        "\nbool read_exact_pmetal_env_source(",
        "exact P_Metal bank resolver",
    )
    require(
        resolver,
        "if (!safe_read(\n                base + 8u,\n                live_version)",
        "safe cached header read",
    )
    require(
        resolver,
        "if (!safe_read(\n            entry,\n            row_id))",
        "safe selected row read",
    )

    source_wrapper = block_between(
        ul_cpp,
        "bool read_exact_pmetal_env_source(",
        "\nbool write_bytes(",
        "legacy safe source wrapper",
    )
    require(
        source_wrapper,
        "if (!safe_read(\n            static_cast<const std::uint8_t *>(\n                source) + 0x18u,\n            base)",
        "safe legacy source->bank pointer read",
    )
    require(
        ul_cpp,
        "if (!safe_read(entry, row_id) ||\n            !safe_read(entry + 8u, name_offset))",
        "safe bank row identity scan",
    )
    require(
        ul_cpp,
        "if (!safe_read(name_byte, ch))",
        "safe bank name identity scan",
    )

    # HemEnvLerp U/L-off remains stock U/L and does not require b13.
    require(
        lerp_cpp,
        "const bool compose_upper_lower =\n        features.enabled(\n            core::operator_id::upper_lower);",
        "Lerp materialization feature split",
    )
    require(
        env_cpp,
        "hemenvlerp &&\n        lerp_upper_lower_composed",
        "runtime b13 only for composed Lerp",
    )

    print("DSRRL_PMETAL_ENVSPEC_REFERENCE_TRANSPORT_PASS")
    print("  producer hot path=source tuple only; no P_Metal FNV/donor decode")
    print("  selected state=exact selector + exact actual P_Metal material")
    print("  source decode=consumer-local after exact P_Metal+EnvSpec gates")
    print("  draw carrier=stable bank_base+selector; producer source object is not dereferenced")
    print("  delayed bank reads=safe_read fail-open")
    print("  dedicated 0x563C30 source hook=disabled")


if __name__ == "__main__":
    main()
