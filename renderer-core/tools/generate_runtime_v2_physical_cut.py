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


    text = replace_once(
        text,
        '''    char ul_direct_line[320]{};
''',
        '''#ifdef DSRRL_FLVER_SELECTOR_PROFILE
    const auto flver_trace =
        dsrrl::runtime::flver_identity_transport::
            selector_profile_stats();
    char flver_trace_line[1280]{};
    std::snprintf(
        flver_trace_line,
        sizeof(flver_trace_line),
        "[DSRRL FLVER TRACE] %s sample=1/%u samples=%llu qpc=%llu total=%llu max=%llu "
        "prefix=%llu resolve=%llu cache_lookup=%llu cache_publish=%llu "
        "owner_lookup=%llu owner_mtd=%llu selection_publish=%llu pmetal=%llu "
        "runtime_mtd=%llu runtime_publish=%llu paths=%llu/%llu/%llu/%llu/%llu "
        "support=%llu/%llu/%llu",
        tag,
        flver_trace.sample_period,
        static_cast<unsigned long long>(flver_trace.samples),
        static_cast<unsigned long long>(flver_trace.qpc_frequency),
        static_cast<unsigned long long>(flver_trace.total_ticks),
        static_cast<unsigned long long>(flver_trace.max_total_ticks),
        static_cast<unsigned long long>(flver_trace.prefix_ticks),
        static_cast<unsigned long long>(flver_trace.resolve_material_ticks),
        static_cast<unsigned long long>(flver_trace.final_cache_lookup_ticks),
        static_cast<unsigned long long>(flver_trace.final_cache_publish_ticks),
        static_cast<unsigned long long>(flver_trace.owner_lookup_ticks),
        static_cast<unsigned long long>(flver_trace.owner_mtd_enrich_ticks),
        static_cast<unsigned long long>(flver_trace.selection_publish_ticks),
        static_cast<unsigned long long>(flver_trace.pmetal_source_ticks),
        static_cast<unsigned long long>(flver_trace.runtime_mtd_lookup_ticks),
        static_cast<unsigned long long>(flver_trace.runtime_publish_ticks),
        static_cast<unsigned long long>(flver_trace.sampled_cache_path),
        static_cast<unsigned long long>(flver_trace.sampled_owner_path),
        static_cast<unsigned long long>(flver_trace.sampled_runtime_mtd_path),
        static_cast<unsigned long long>(flver_trace.sampled_fail_open_path),
        static_cast<unsigned long long>(flver_trace.sampled_early_reject_path),
        static_cast<unsigned long long>(flver_trace.parse_events),
        static_cast<unsigned long long>(flver_trace.mtd_events),
        static_cast<unsigned long long>(flver_trace.destroy_events));
    reshade::log::message(
        reshade::log::level::info,
        flver_trace_line);
#endif

    char ul_direct_line[320]{};
''',
        "FLVER selector profile log insertion")

    text = replace_once(
        text,
        '''    if (present == 1u ||
        (g_hot_telemetry_enabled &&
         (present % 300u) == 0u)) {
''',
        '''    if (present == 1u ||
#ifdef DSRRL_FLVER_SELECTOR_PROFILE
        (present % 300u) == 0u ||
#endif
        (g_hot_telemetry_enabled &&
         (present % 300u) == 0u)) {
''',
        "FLVER selector profile periodic present log")

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
