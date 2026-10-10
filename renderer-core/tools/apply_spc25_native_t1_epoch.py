#!/usr/bin/env python3
"""Apply/verify the bounded SPC25 PS-t1 lifetime observer at source level.

Committed as a separate exact-source recipe so the oversized legacy source
files need not be rewritten by an API transport with per-request size limits.
Never changes DSR EXE, GPU state, PTDE sidecars, or retail shipping configuration.
"""
from pathlib import Path
import argparse

ROOT = Path(__file__).resolve().parents[2]
TEX = ROOT / "renderer-core/src/runtime/texture_identity_transport.cpp"
MAT = ROOT / "renderer-core/src/runtime/material_resource_draw_runtime.cpp"

TEXTURE = [
 ('#include "dsrrl/runtime/texture_identity_transport.hpp"',
  '#include "dsrrl/runtime/texture_identity_transport.hpp"\n#include "dsrrl/runtime/native_ps_t1_view_lifetime.hpp"'),
 ('    std::uint32_t source_id = 0u;\n    std::array<char,65u> ascii_name{};',
  '    std::uint32_t source_id = 0u;\n    std::uint64_t epoch = 0u;\n    std::array<char,65u> ascii_name{};'),
 ('                slot.source_id = source_id;\n                const auto copy_length',
  '                slot.source_id = source_id;\n                slot.epoch = native_ps_t1_lifetime::instance().current(\n                    managed_fields[1],managed_fields[0]).epoch;\n                const auto copy_length'),
 ('    std::uint32_t matches = 0u;\n    std::uint32_t full_matches = 0u;\n    const native_named_source *first = nullptr;',
  '    const auto live_epoch = native_ps_t1_lifetime::instance().current(\n        native_srv,native_tex);\n    std::uint32_t matches = 0u;\n    std::uint32_t full_matches = 0u;\n    std::uint32_t epoch_matches = 0u;\n    const native_named_source *first = nullptr;'),
 ('        if(slot.texture == native_tex)\n            ++full_matches;',
  '        if(slot.texture == native_tex) {\n            ++full_matches;\n            if(live_epoch.epoch && slot.epoch == live_epoch.epoch)\n                ++epoch_matches;\n        }'),
 ('        "cross_epoch_lifetime=OPEN reuse_collision=UNVERIFIED "\n        "bridge_authority=0 srv_swap=0 pixel=OPEN",',
  '        "matching_epoch=%u live_epoch=%llu writer_epoch=%llu "\n        "cross_epoch_lifetime=OPEN reuse_collision=UNVERIFIED "\n        "bridge_authority=0 srv_swap=0 pixel=OPEN",'),
 ('        first ? first->source_id : 0u);',
  '        first ? first->source_id : 0u,\n        epoch_matches,\n        static_cast<unsigned long long>(live_epoch.epoch),\n        static_cast<unsigned long long>(first ? first->epoch : 0u));'),
 ("    const auto writer_seq = g_cpu_packet_writer_calls.fetch_add(\n        1u, std::memory_order_relaxed);\n",
  "    const auto writer_seq = g_cpu_packet_writer_calls.fetch_add(\n        1u, std::memory_order_relaxed);\n    // The owner may rotate armor after the 1024th snapshot. Report\n    // truncation exactly once instead of misreporting late sources absent.\n    if (writer_seq == g_native_named_sources.size())\n        reshade::log::message(reshade::log::level::warning,\n            \"[DSRRL SPC25 COVERAGE] stage=writer_snapshot_capacity \"\n            \"writer_capacity=1024 late_cpu_writer_source_unobservable=1 \"\n            \"diagnostic_only=1 srv_swap=0 pixel=OPEN\");\n"),
 ("    if(retired &&\n       g_native_t1_retire_logs.fetch_add(\n           1u,std::memory_order_relaxed) < 16u) {",
  "    const auto retire_index = retired ?\n        g_native_t1_retire_logs.fetch_add(\n            1u,std::memory_order_relaxed) + 1u : 0u;\n    // Preserve late unload/reload evidence without unbounded logging.\n    if (retired && (retire_index <= 16u ||\n        (retire_index <= 8192u &&\n         (retire_index & (retire_index - 1u)) == 0u))) {"),
 ("    const bool should_log = matches\n        ? g_native_t1_match_logs.fetch_add(\n              1u,std::memory_order_relaxed) < 32u\n        : g_native_t1_no_match_logs.fetch_add(\n              1u,std::memory_order_relaxed) < 8u;\n    if (!should_log)\n        return;",
  "    const auto sample_index = matches ?\n        g_native_t1_match_logs.fetch_add(\n            1u,std::memory_order_relaxed) + 1u :\n        g_native_t1_no_match_logs.fetch_add(\n            1u,std::memory_order_relaxed) + 1u;\n    // First N + powers-of-two telemetry persists across armor rotations\n    // and map reload. Always record exact target c5330_s if encountered.\n    const bool target_c5330 = first &&\n        std::strcmp(first->ascii_name.data(),\"c5330_s\") == 0;\n    const bool should_log = target_c5330 ||\n        sample_index <= (matches ? 32u : 8u) ||\n        (sample_index <= 8192u &&\n         (sample_index & (sample_index - 1u)) == 0u);\n    if (!should_log)\n        return;"),
 ("    if(count <= 24u || (count & (count-1u)) == 0u) {",
  "    const bool is_c5330 = length == 7u &&\n        std::char_traits<wchar_t>::compare(name,L\"c5330_s\",7u) == 0;\n    // Always expose exact targeted CPU source, even past early log budget.\n    if(count <= 24u || (count & (count-1u)) == 0u || is_c5330) {"),
 ("std::atomic<std::uint32_t> g_native_t1_match_logs{0u};",
  "std::atomic<std::uint32_t> g_native_t1_match_logs{0u};\n// Target-only CPU writer pointer; never owned, dereferenced, or bound.\n// Cleared on destruction; new generation must come from a new CPU writer.\nstd::atomic<std::uintptr_t> g_spc25_c5330_srv{0u};\nstd::atomic<std::uint64_t> g_spc25_c5330_probe_draws{0u};\nstd::atomic<std::uint64_t> g_spc25_c5330_probe_hits{0u};"),
 ("                slot.published_srv.store(\n                    managed_fields[1],std::memory_order_release);",
  "                slot.published_srv.store(\n                    managed_fields[1],std::memory_order_release);\n                if (length == 7u &&\n                    std::char_traits<wchar_t>::compare(\n                        name,L\"c5330_s\",7u) == 0)\n                    g_spc25_c5330_srv.store(\n                        managed_fields[1],std::memory_order_release);"),
 ("    if(retired)\n        g_native_t1_writer_retired.fetch_add(",
  "    auto expected_c5330_srv = key;\n    (void)g_spc25_c5330_srv.compare_exchange_strong(\n        expected_c5330_srv,0u,std::memory_order_acq_rel,\n        std::memory_order_acquire);\n    if(retired)\n        g_native_t1_writer_retired.fetch_add("),
 ("void diagnose_native_ps_t1(\n    const void *native_view,\n    const void *native_resource) noexcept\n{",
  "bool should_probe_c5330_ps_t1(\n    const void *native_view, std::uint32_t receiver_id) noexcept\n{\n#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)\n    const auto source_srv = g_spc25_c5330_srv.load(\n        std::memory_order_acquire);\n    if (!source_srv || !native_view)\n        return false;\n    const auto view = reinterpret_cast<std::uintptr_t>(native_view);\n    const auto checks = g_spc25_c5330_probe_draws.fetch_add(\n        1u,std::memory_order_relaxed) + 1u;\n    const bool same_view = view == source_srv;\n    const auto hits = same_view ?\n        g_spc25_c5330_probe_hits.fetch_add(\n            1u,std::memory_order_relaxed) + 1u :\n        g_spc25_c5330_probe_hits.load(std::memory_order_relaxed);\n    const bool check_milestone = checks <= 65536u &&\n        (checks == 1u || (checks & (checks - 1u)) == 0u);\n    const bool hit_milestone = same_view &&\n        hits <= 65536u &&\n        (hits == 1u || (hits & (hits - 1u)) == 0u);\n    if (check_milestone || hit_milestone) {\n        char msg[384]{};\n        std::snprintf(msg,sizeof(msg),\n            \"[DSRRL SPC25 C5330 T1 COVERAGE] stage=eligible_ps_t1 \"\n            \"rx=%u cpu_source_seen=1 target_srv=%p bound_srv=%p \"\n            \"exact_srv=%u checked_draws=%llu target_hits=%llu \"\n            \"bridge_authority=0 srv_swap=0 pixel=OPEN\",\n            receiver_id,\n            reinterpret_cast<const void *>(source_srv),native_view,\n            same_view ? 1u : 0u,\n            static_cast<unsigned long long>(checks),\n            static_cast<unsigned long long>(hits));\n        reshade::log::message(reshade::log::level::info,msg);\n    }\n    // The pinned resource is inspected only on matching-source hits,\n    // and only at a bounded logarithmic frequency. First-sample\n    // diagnostics continue to run for all other native views.\n    return hit_milestone;\n#else\n    (void)native_view;\n    (void)receiver_id;\n    return false;\n#endif\n}\n\nvoid diagnose_native_ps_t1(\n    const void *native_view,\n    const void *native_resource) noexcept\n{"),
 ("void diagnose_native_ps_t1(\n    const void *native_view,\n    const void *native_resource) noexcept\n{",
  "bool match_exact_c5330_native_ps_t1(\n    const void *native_view, const void *native_resource) noexcept\n{\n#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)\n    const auto srv = reinterpret_cast<std::uintptr_t>(native_view);\n    const auto tex = reinterpret_cast<std::uintptr_t>(native_resource);\n    if (!srv || !tex)\n        return false;\n    const auto epoch = native_ps_t1_lifetime::instance().current(\n        srv,tex).epoch;\n    if (!epoch)\n        return false;\n    std::uint32_t count = 0u;\n    for (const auto &slot : g_native_named_sources) {\n        if (slot.published_srv.load(std::memory_order_acquire) != srv)\n            continue;\n        if (slot.texture != tex || slot.epoch != epoch ||\n            std::strcmp(slot.ascii_name.data(),\"c5330_s\") != 0)\n            return false;\n        if (++count > 1u)\n            return false;\n    }\n    return count == 1u;\n#else\n    (void)native_view;\n    (void)native_resource;\n    return false;\n#endif\n}\n\nvoid diagnose_native_ps_t1(\n    const void *native_view,\n    const void *native_resource) noexcept\n{"),
]

MATERIAL = [
 ('#include "dsrrl/runtime/spc25_physical_t1_probe.hpp"',
  '#include "dsrrl/runtime/spc25_physical_t1_probe.hpp"\n#include "dsrrl/runtime/native_ps_t1_view_lifetime.hpp"'),
 ('    spc25_physical::link_view(view.handle,resource.handle);',
  '    spc25_physical::link_view(view.handle,resource.handle);\n    native_ps_t1_lifetime::instance().init(\n        static_cast<std::uintptr_t>(device->get_native()),\n        static_cast<std::uintptr_t>(view.handle),\n        static_cast<std::uintptr_t>(resource.handle));'),
 ('void on_destroy_resource_view(\n    reshade::api::device *,',
  'void on_destroy_resource_view(\n    reshade::api::device *device,'),
 ('    texture_identity_transport::retire_native_ps_t1(\n',
  '    if (device && device->get_api() == reshade::api::device_api::d3d11)\n        native_ps_t1_lifetime::instance().destroy(\n            static_cast<std::uintptr_t>(device->get_native()),\n            static_cast<std::uintptr_t>(view.handle));\n    texture_identity_transport::retire_native_ps_t1(\n'),
 ('    spc25_physical::drop_device(native);',
  '    native_ps_t1_lifetime::instance().destroy_device(\n        reinterpret_cast<std::uintptr_t>(native));\n    spc25_physical::drop_device(native);'),
 ("        if (views[1] != nullptr &&\n            texture_identity_transport::should_sample_native_ps_t1(\n                views[1])) {",
  "        const bool ordinary_sample = views[1] != nullptr &&\n            texture_identity_transport::should_sample_native_ps_t1(\n                views[1]);\n        const bool target_sample = views[1] != nullptr &&\n            texture_identity_transport::should_probe_c5330_ps_t1(\n                views[1],receiver_id);\n        if (ordinary_sample || target_sample) {"),
 ("                texture_identity_transport::diagnose_native_ps_t1(\n                    views[1], stock_texture);\n                stock_texture->Release();",
  "                texture_identity_transport::diagnose_native_ps_t1(\n                    views[1], stock_texture);\n                // Source identity and resource-generation equality are\n                // already proven here, but the existing PTDE sidecar\n                // companion cache has no logical-name mapping for t1.\n                // Inspect exact file presence only, never bind/load it.\n                if (receiver_id == 34u &&\n                    texture_identity_transport::\n                        match_exact_c5330_native_ps_t1(\n                            views[1], stock_texture)) {\n                    static std::atomic_bool logged{false};\n                    if (!logged.exchange(\n                            true,std::memory_order_relaxed)) {\n                        bool present = false;\n                        bool whitelist = false;\n                        bool path_error = false;\n                        try {\n                            const auto path = sidecar_path(\n                                asset_class::specular,L\"c5330_s\");\n                            std::error_code ec{};\n                            present = std::filesystem::is_regular_file(\n                                path,ec) && !ec;\n                            path_error = bool(ec);\n                            whitelist =\n                                generated::spec_equipment_name_hash_allowed_v12(\n                                    fnv_name(L\"c5330_s\",7u));\n                        } catch (...) {\n                            path_error = true;\n                        }\n                        char msg[384]{};\n                        std::snprintf(msg,sizeof(msg),\n                            \"[DSRRL SPC25 C5330 SIDECAR CUT] \"\n                            \"rx=34 source=c5330_s \"\n                            \"native_srv_resource_epoch=EXACT \"\n                            \"spec_v12_allowlist=%u spec_dds_file_present=%u \"\n                            \"path_error=%u existing_companion_cache=\"\n                            \"UNRESOLVED sidecar_bytes=UNVERIFIED \"\n                            \"bridge_authority=0 srv_swap=0 pixel=OPEN\",\n                            whitelist ? 1u : 0u,\n                            present ? 1u : 0u,\n                            path_error ? 1u : 0u);\n                        reshade::log::message(\n                            reshade::log::level::info,msg);\n                    }\n                }\n                stock_texture->Release();"),
]

def process(path: Path, rules, write: bool) -> None:
    text = path.read_text(encoding="utf-8")
    for before, after in rules:
        if text.count(after) == 1:
            continue
        if text.count(before) != 1:
            raise RuntimeError(f"Source drift/conflict: {path.name} / {before[:55]!r}")
        if not write:
            raise RuntimeError(f"SPC25 lifetime change missing: {path.name} / {before[:55]!r}")
        text = text.replace(before, after, 1)
    if write:
        path.write_text(text, encoding="utf-8", newline="")
    print(f"SPC25_EPOCH_{'APPLIED' if write else 'VERIFIED'} {path.name}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    process(TEX, TEXTURE, args.apply)
    process(MAT, MATERIAL, args.apply)
    print("SPC25_EPOCH_DIAGNOSTIC_ONLY: bridge_authority=0 srv_swap=0")

if __name__ == "__main__":
    main()
