#!/usr/bin/env python3
import argparse
from pathlib import Path
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--cmake", required=True)
    args = parser.parse_args()

    source_dir = Path(args.source_dir).resolve()
    cmake_path = Path(args.cmake).resolve()
    pointlight_dir = source_dir / "src" / "operators" / "point_light"

    cmake_text = cmake_path.read_text(encoding="utf-8")
    sources = sorted(pointlight_dir.glob("*.cpp"))

    missing = []
    for path in sources:
        rel = path.relative_to(source_dir).as_posix()
        if rel not in cmake_text:
            missing.append(rel)

    if missing:
        print("PointLight core source audit: missing CMake entries:", file=sys.stderr)
        for rel in missing:
            print(f"  {rel}", file=sys.stderr)
        return 1

    # Runtime architecture invariant: PointLight owns its selector callback
    # directly from the FLVER semantic cut. It must not depend on the
    # Upper/Lower runtime merely to receive owner identity.
    flver_hooks = (
        source_dir / "src" / "runtime" / "flver_engine_hooks.cpp"
    ).read_text(encoding="utf-8")
    upper_lower = (
        source_dir / "src" / "runtime" / "upper_lower_draw_runtime.cpp"
    ).read_text(encoding="utf-8")
    integrated = (
        source_dir / "integrated" / "integrated_addon.cpp"
    ).read_text(encoding="utf-8")

    required_flver = (
        '#include "dsrrl/runtime/fixed_pointlight_draw_runtime.hpp"',
        "fixed_pointlight_selector_event_bridge(owner);",
    )
    for token in required_flver:
        if token not in flver_hooks:
            print(
                f"PointLight runtime audit: FLVER direct dispatch missing: {token}",
                file=sys.stderr,
            )
            return 1

    # Clustered PointLight is intentionally source-only on the R43 PARAM line.
    # FLVER/material receiver dispatch is forbidden; stock DSR consumes the
    # corrected source carrier.
    for token in (
        "clustered_pnts_selector_event_bridge(",
        "clustered_pnts_selector_source_event_bridge(",
        "clustered_pnts_selector_identity_event_bridge(",
    ):
        if token in flver_hooks:
            print(
                f"PointLight runtime audit: Clustered receiver dispatch survived: {token}",
                file=sys.stderr,
            )
            return 1

    clustered_draw = (
        source_dir / "src" / "runtime" /
        "clustered_pnts_draw_runtime.cpp"
    ).read_text(encoding="utf-8")
    for token in (
        "k_clustered_source_override_rva = 0xB7E02u;",
        "capture_clustered_live_drawparam_source(",
        "g_source_dsr_only_fail_open",
    ):
        if token not in clustered_draw:
            print(
                f"PointLight runtime audit: Clustered live-DrawParam source invariant missing: {token}",
                file=sys.stderr,
            )
            return 1

    callback_begin = clustered_draw.find(
        "float __fastcall clustered_source_override_callback("
    )
    callback_end = clustered_draw.find(
        "bool build_clustered_source_override_stub(",
        callback_begin,
    )
    if callback_begin < 0 or callback_end <= callback_begin:
        print(
            "PointLight runtime audit: cannot isolate Clustered source callback",
            file=sys.stderr,
        )
        return 1
    callback = clustered_draw[callback_begin:callback_end]
    if "pointlight_ptde_source::capture(" in callback:
        print(
            "PointLight runtime audit: Clustered source callback still uses embedded donor lookup",
            file=sys.stderr,
        )
        return 1

    bridge_begin = upper_lower.find(
        "void upper_lower_selector_event_bridge("
    )
    bridge_end = upper_lower.find(
        "\nvoid upper_lower_pmetal_material_event_bridge(",
        bridge_begin,
    )
    if bridge_begin < 0 or bridge_end <= bridge_begin:
        print(
            "PointLight runtime audit: cannot isolate U/L selector bridge",
            file=sys.stderr,
        )
        return 1
    if "fixed_pointlight_selector_event_bridge" in upper_lower[
        bridge_begin:bridge_end
    ]:
        print(
            "PointLight runtime audit: Fixed PL still routed through U/L selector bridge",
            file=sys.stderr,
        )
        return 1

    fast_begin = integrated.find(
        "bool observe_pointlight_draw_identity("
    )
    fast_end = integrated.find(
        "\nbool observe_draw_identity(",
        fast_begin,
    )
    if fast_begin < 0 or fast_end <= fast_begin:
        print(
            "PointLight runtime audit: exact-route draw fast path missing",
            file=sys.stderr,
        )
        return 1

    fast_path = integrated[fast_begin:fast_end]
    forbidden_generic_receivers = (
        "stable_receiver_bound(",
        "hemenvlerp_receiver_bound(",
        "subsurface_receiver_bound(",
        "hemdir3_receiver_bound(",
        "upper_lower_receiver_bound(",
    )
    for token in forbidden_generic_receivers:
        if token in fast_path:
            print(
                f"PointLight runtime audit: fast path regressed into generic receiver census: {token}",
                file=sys.stderr,
            )
            return 1

    if integrated.count("observe_pointlight_draw_identity(") < 3:
        print(
            "PointLight runtime audit: both draw dispatch sites must use the fast path",
            file=sys.stderr,
        )
        return 1

    # Bind state-shadow invariant: exact PointLight pipeline identity lives on
    # the ReShade command-list object and is cached per thread. Global
    # command->record maps are forbidden; the synchronized pipeline registry is
    # consulted only when the epoch-invalidated attestation cache misses.
    fixed_pipe_h = (
        source_dir / "include" / "dsrrl" / "runtime" /
        "fixed_pointlight_pipeline_runtime.hpp"
    ).read_text(encoding="utf-8")
    clustered_pipe_h = (
        source_dir / "include" / "dsrrl" / "runtime" /
        "clustered_pnts_pipeline_runtime.hpp"
    ).read_text(encoding="utf-8")
    fixed_pipe_cpp = (
        source_dir / "src" / "runtime" /
        "fixed_pointlight_pipeline_runtime.cpp"
    ).read_text(encoding="utf-8")
    clustered_pipe_cpp = (
        source_dir / "src" / "runtime" /
        "clustered_pnts_pipeline_runtime.cpp"
    ).read_text(encoding="utf-8")
    fixed_draw_cpp = (
        source_dir / "src" / "runtime" /
        "fixed_pointlight_draw_runtime.cpp"
    ).read_text(encoding="utf-8")

    for label, header in (
        ("fixed", fixed_pipe_h),
        ("clustered", clustered_pipe_h),
    ):
        state_begin = header.find("struct bound_tls_state")
        state_end = header.find("struct attestation_tls_entry", state_begin)
        if state_begin < 0 or state_end <= state_begin:
            print(
                f"PointLight runtime audit: cannot isolate {label} bound TLS state",
                file=sys.stderr,
            )
            return 1
        state = header[state_begin:state_end]
        for token in (
            "std::uint64_t pipeline = 0u;",
            "std::uint64_t pipeline_epoch = 0u;",
        ):
            if token not in state:
                print(
                    f"PointLight runtime audit: {label} TLS state-shadow identity missing: {token}",
                    file=sys.stderr,
                )
                return 1

        for forbidden in (
            "std::shared_ptr<const record>> bound_;",
            "bound_epoch_",
            "any_bound_",
        ):
            if forbidden in header:
                print(
                    f"PointLight runtime audit: {label} global command binding state survived: {forbidden}",
                    file=sys.stderr,
                )
                return 1

    for label, source, signature, destroy_sig in (
        (
            "fixed",
            fixed_pipe_cpp,
            "bool fixed_pointlight_pipeline_runtime::on_bind_pipeline(",
            "\nvoid fixed_pointlight_pipeline_runtime::on_destroy_pipeline(",
        ),
        (
            "clustered",
            clustered_pipe_cpp,
            "bool clustered_pnts_pipeline_runtime::on_bind_pipeline(",
            "\nvoid clustered_pnts_pipeline_runtime::on_destroy_pipeline(",
        ),
    ):
        bind_begin = source.find(signature)
        bind_end = source.find(destroy_sig, bind_begin)
        if bind_begin < 0 or bind_end <= bind_begin:
            print(
                f"PointLight runtime audit: cannot isolate {label} bind path",
                file=sys.stderr,
            )
            return 1
        bind = source[bind_begin:bind_end]
        tls_reuse = bind.find("bound_tls_.pipeline == pipeline.handle")
        registry_lookup = bind.find("pipeline_attested_cached(")
        if (
            tls_reuse < 0
            or registry_lookup < 0
            or tls_reuse >= registry_lookup
        ):
            print(
                f"PointLight runtime audit: {label} repeated bind does not reuse TLS before synchronized registry lookup",
                file=sys.stderr,
            )
            return 1
        if "set_private_data(" not in bind:
            print(
                f"PointLight runtime audit: {label} bind does not publish object-local command state",
                file=sys.stderr,
            )
            return 1

    for label, source, key in (
        ("fixed", fixed_pipe_cpp, "k_fixed_pointlight_binding_guid"),
        ("clustered", clustered_pipe_cpp, "k_clustered_pointlight_binding_guid"),
    ):
        if source.count("get_private_data(") < 2 or key not in source:
            print(
                f"PointLight runtime audit: {label} draw path is not command-local state backed",
                file=sys.stderr,
            )
            return 1

    capture_begin = fixed_draw_cpp.find(
        "void __fastcall capture_callback("
    )
    capture_end = fixed_draw_cpp.find(
        "\nbool build_capture_stub(",
        capture_begin,
    )
    if capture_begin < 0 or capture_end <= capture_begin:
        print(
            "PointLight runtime audit: cannot isolate fixed producer capture",
            file=sys.stderr,
        )
        return 1
    capture = fixed_draw_cpp[capture_begin:capture_end]
    publish_gate = capture.find("const bool publishable_count =")
    map_lock = capture.find("std::lock_guard<std::mutex> lock(g_mutex);")
    if (
        publish_gate < 0
        or "current->captured_count==2u" not in capture
        or "current->captured_count==4u" not in capture
        or map_lock < 0
        or publish_gate >= map_lock
    ):
        print(
            "PointLight runtime audit: fixed producer publishes partial 1/3-light snapshots or locks before completion gate",
            file=sys.stderr,
        )
        return 1

    selector_begin = fixed_draw_cpp.find(
        "void fixed_pointlight_draw_runtime::selector_event("
    )
    selector_end = fixed_draw_cpp.find(
        "\nbool fixed_pointlight_draw_runtime::prepare_t19(",
        selector_begin,
    )
    if selector_begin < 0 or selector_end <= selector_begin:
        print(
            "PointLight runtime audit: cannot isolate fixed selector path",
            file=sys.stderr,
        )
        return 1
    selector = fixed_draw_cpp[selector_begin:selector_end]
    producer_tls = selector.find("const auto producer=g_producer_snapshot;")
    sync_fallback = selector.find(
        "std::lock_guard<std::mutex> lock(g_mutex);"
    )
    if (
        producer_tls < 0
        or "g_draw_snapshot=producer;" not in selector
        or sync_fallback < 0
        or producer_tls >= sync_fallback
    ):
        print(
            "PointLight runtime audit: fixed selector does not use producer TLS before synchronized owner-map fallback",
            file=sys.stderr,
        )
        return 1

    print(
        f"PointLight core source audit: PASS ({len(sources)} implementation units; "
        "dedicated selector + exact-route draw fast path + object-local/TLS state shadow)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
