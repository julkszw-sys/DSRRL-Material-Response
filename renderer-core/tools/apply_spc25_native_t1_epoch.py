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
]

MATERIAL = [
 ('#include "dsrrl/runtime/spc25_physical_t1_probe.hpp"',
  '#include "dsrrl/runtime/spc25_physical_t1_probe.hpp"\n#include "dsrrl/runtime/native_ps_t1_view_lifetime.hpp"'),
 ('    spc25_physical::link_view(view.handle,resource.handle);',
  '    spc25_physical::link_view(view.handle,resource.handle);\n    native_ps_t1_lifetime::instance().init(\n        reinterpret_cast<std::uintptr_t>(device->get_native()),\n        static_cast<std::uintptr_t>(view.handle),\n        static_cast<std::uintptr_t>(resource.handle));'),
 ('void on_destroy_resource_view(\n    reshade::api::device *,',
  'void on_destroy_resource_view(\n    reshade::api::device *device,'),
 ('    texture_identity_transport::retire_native_ps_t1(\n',
  '    if (device && device->get_api() == reshade::api::device_api::d3d11)\n        native_ps_t1_lifetime::instance().destroy(\n            reinterpret_cast<std::uintptr_t>(device->get_native()),\n            static_cast<std::uintptr_t>(view.handle));\n    texture_identity_transport::retire_native_ps_t1(\n'),
 ('    spc25_physical::drop_device(native);',
  '    native_ps_t1_lifetime::instance().destroy_device(\n        reinterpret_cast<std::uintptr_t>(native));\n    spc25_physical::drop_device(native);'),
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
