#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path


STUBS = r'''
// DSRRL PHYSICAL_CUT_UL_H3_SUBSURFACE generated source.
// Compile-only fail-open shims: no removed U/L, HemDir3 or Subsurface runtime
// object, hook owner, mutex/map state or GPU resource owner is instantiated.
struct physical_cut_upper_lower_runtime_stub {
    bool install(bool = false) noexcept { return false; }
    void uninstall() noexcept {}
    bool direct_producer_active() const noexcept { return false; }
    bool direct_producer_ready_for_draw() const noexcept { return false; }
    bool prepare_upper_lower_carrier(
        ID3D11DeviceContext *,
        dsrrl::runtime::prepared_upper_lower_draw &) noexcept { return false; }
    void consume_draw_selection() noexcept {}
    void on_destroy_device(reshade::api::device *) noexcept {}
    dsrrl::runtime::upper_lower_telemetry telemetry() const noexcept { return {}; }
    void reset() noexcept {}
};

struct physical_cut_subsurface_runtime_stub {
    bool prepare(
        reshade::api::command_list *,
        const dsrrl::operators::material_response::material_identity &,
        dsrrl::runtime::prepared_subsurface_draw &) noexcept { return false; }
    void release(dsrrl::runtime::prepared_subsurface_draw &prepared) noexcept {
        prepared = {};
    }
    dsrrl::runtime::subsurface_draw_telemetry telemetry() const noexcept { return {}; }
    void reset() noexcept {}
};

struct physical_cut_upper_lower_hemenv_runtime_stub {
    void on_init_device(reshade::api::device *) noexcept {}
    void on_destroy_device(reshade::api::device *) noexcept {}
    template <typename Outcome>
    bool register_replacement(
        const Outcome &, const void *, std::size_t) noexcept { return false; }
    bool prepare_draw_request(
        reshade::api::command_list *,
        const dsrrl::runtime::upper_lower_receiver_identity &,
        bool,
        dsrrl::runtime::prepared_upper_lower_hemenv_draw &) noexcept { return false; }
    void release_prepared_draw(
        dsrrl::runtime::prepared_upper_lower_hemenv_draw &prepared) noexcept {
        prepared = {};
    }
    dsrrl::runtime::upper_lower_hemenv_draw_telemetry telemetry() const noexcept {
        return {};
    }
    void reset() noexcept {}
};

struct physical_cut_hemdir3_runtime_stub {
    void on_init_device(reshade::api::device *) noexcept {}
    void on_destroy_device(reshade::api::device *) noexcept {}
    template <typename Outcome>
    bool register_replacement(
        const Outcome &, const void *, std::size_t) noexcept { return false; }
    bool prepare_draw_request(
        reshade::api::command_list *,
        const dsrrl::runtime::hemdir3_receiver_identity &,
        const dsrrl::operators::material_response::material_identity &,
        dsrrl::runtime::prepared_hemdir3_draw &) noexcept { return false; }
    void release_prepared_draw(
        dsrrl::runtime::prepared_hemdir3_draw &prepared) noexcept {
        prepared = {};
    }
    dsrrl::runtime::hemdir3_draw_telemetry telemetry() const noexcept { return {}; }
    void reset() noexcept {}
};
'''

ORIGINAL_GLOBALS = '''dsrrl::runtime::upper_lower_draw_runtime
    g_upper_lower(g_core);
dsrrl::runtime::pmetal_env_source_runtime
    g_pmetal_source;
dsrrl::runtime::subsurface_draw_runtime
    g_subsurface(
        g_core,
        g_mr_draw_runtime,
        g_material_resources);
dsrrl::runtime::upper_lower_hemenv_draw_runtime
    g_upper_lower_hemenv(g_core, g_upper_lower);
dsrrl::runtime::hemdir3_draw_runtime
    g_hemdir3(g_core, g_upper_lower);
'''

PHYSICAL_GLOBALS = '''physical_cut_upper_lower_runtime_stub
    g_upper_lower;
physical_cut_subsurface_runtime_stub
    g_subsurface;
physical_cut_upper_lower_hemenv_runtime_stub
    g_upper_lower_hemenv;
physical_cut_hemdir3_runtime_stub
    g_hemdir3;
dsrrl::runtime::pmetal_env_source_runtime
    g_pmetal_source;
'''


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one anchor, found {count}")
    return text.replace(old, new, 1)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True, type=Path)
    ap.add_argument("--output", required=True, type=Path)
    args = ap.parse_args()

    text = args.input.read_text(encoding="utf-8")
    text = replace_once(
        text,
        "namespace {\n\ndsrrl::core::renderer_core g_core;",
        "namespace {\n" + STUBS + "\ndsrrl::core::renderer_core g_core;",
        "stub insertion")
    text = replace_once(
        text,
        ORIGINAL_GLOBALS,
        PHYSICAL_GLOBALS,
        "runtime global replacement")

    for forbidden in (
        "dsrrl::runtime::upper_lower_draw_runtime\n    g_upper_lower",
        "dsrrl::runtime::subsurface_draw_runtime\n    g_subsurface",
        "dsrrl::runtime::upper_lower_hemenv_draw_runtime\n    g_upper_lower_hemenv",
        "dsrrl::runtime::hemdir3_draw_runtime\n    g_hemdir3",
    ):
        if forbidden in text:
            raise SystemExit(f"real runtime global survived physical cut: {forbidden}")

    if "dsrrl::runtime::pmetal_env_source_runtime\n    g_pmetal_source;" not in text:
        raise SystemExit("P_Metal source runtime was not preserved")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="utf-8", newline="\n")
    print(f"PHYSICAL_CUT_SOURCE_READY={args.output}")


if __name__ == "__main__":
    main()
