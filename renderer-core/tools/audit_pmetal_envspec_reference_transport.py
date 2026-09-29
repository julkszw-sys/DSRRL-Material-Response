#!/usr/bin/env python3
import argparse
from pathlib import Path


def fail(msg: str) -> None:
    raise RuntimeError(msg)


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        fail(f"{label}: missing {needle!r}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source-dir", required=True)
    ns = ap.parse_args()
    root = Path(ns.source_dir).resolve()

    integrated = (root / "integrated/integrated_addon.cpp").read_text(encoding="utf-8")
    ul_h = (root / "include/dsrrl/runtime/upper_lower_draw_runtime.hpp").read_text(encoding="utf-8")
    ul_cpp = (root / "src/runtime/upper_lower_draw_runtime.cpp").read_text(encoding="utf-8")
    env_cpp = (root / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")

    # P_Metal source transport is independent of visible U/L activation.
    require(
        integrated,
        "const bool pmetal_envspec_enabled =",
        "P_Metal EnvSpec feature gate",
    )
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

    # Installing the shared carrier must never silently re-enable U/L inside
    # the EnvSpec composite.
    require(
        integrated,
        "const bool envspec_ul_verified =\n        g_core.features().enabled(\n            dsrrl::core::operator_id::upper_lower)",
        "EnvSpec U/L composition feature isolation",
    )

    # Reference-only mode must carry exact source/selector tuples but avoid
    # heavy U/L/D123 evaluation on the producer hot path.
    require(
        ul_h,
        "bool install(bool reference_only = false) noexcept;",
        "reference-only API",
    )
    require(
        ul_cpp,
        "g_reference_only_transport",
        "reference-only runtime flag",
    )
    require(
        ul_cpp,
        "P_Metal EnvSpec requires only the exact source/selector",
        "steady reference-only branch",
    )
    require(
        ul_cpp,
        "if (g_reference_only_transport.load(\n                    std::memory_order_acquire))\n                return result;",
        "blend reference-only branch",
    )

    # The old producer-local P_Metal semantic decode stays disabled. EnvSpec
    # now resolves the source lazily only after the exact draw consumer gate.
    require(
        ul_cpp,
        "g_pmetal_env_hook_armed.store(false);",
        "no shared producer-local P_Metal decode",
    )
    require(
        env_cpp,
        "selected_pmetal_env_source(",
        "consumer-local P_Metal source decode",
    )
    require(
        env_cpp,
        "k_effect_fail_source = 1u << 4u;",
        "source frontier telemetry",
    )

    print("DSRRL_PMETAL_ENVSPEC_REFERENCE_TRANSPORT_PASS")
    print("  U/L visible operator=OFF remains stock when not explicitly enabled")
    print("  LightBank reference carrier=ON for P_Metal EnvSpec")
    print("  producer payload=owner+sourceA/B+selectorA/B+beta only")
    print("  P_Metal source decode=consumer-local after exact material/semantic gate")


if __name__ == "__main__":
    main()
