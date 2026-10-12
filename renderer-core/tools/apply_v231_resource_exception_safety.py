#!/usr/bin/env python3
"""V231 generated safety patch. Apply only after V203/227/229 and V230 IO.
No DXBC, receiver routing, material operator or visual output changes.
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TARGET = ROOT / "renderer-core/src/runtime/material_resource_draw_runtime.cpp"

RELEASE_OLD = "void release_cache_for_device(\n    ID3D11Device *device) noexcept\n{\n    if (device == nullptr)\n        return;\n\n    std::vector<companion_set> dead;\n\n    {\n        std::lock_guard<std::mutex> lock(g_mutex);\n\n        bool changed = false;\n        for (auto it = g_cache.begin();\n             it != g_cache.end();) {\n            if (it->second.device == device) {\n                dead.push_back(it->second);\n                it = g_cache.erase(it);\n                changed = true;\n            } else {\n                ++it;\n            }\n        }\n\n        for (auto it = g_ambiguous_view_device.begin();\n             it != g_ambiguous_view_device.end();) {\n            if (it->second == device) {\n                it = g_ambiguous_view_device.erase(it);\n                changed = true;\n            } else {\n                ++it;\n            }\n        }\n\n        if (changed)\n            invalidate_all_companion_epochs();\n    }\n\n    for (auto &set : dead)\n        release_set(set);\n}\n\n"
RELEASE_NEW = "void release_cache_for_device(\n    ID3D11Device *device) noexcept\n{\n    if (device == nullptr)\n        return;\n\n    // Detach at most 64 COM-owned companions under the mutex, then Release\n    // outside it. A heap-allocated vector here can throw through noexcept\n    // during device teardown and terminate the host process.\n    constexpr std::size_t k_release_batch = 64u;\n    for (;;) {\n        std::array<companion_set, k_release_batch> dead{};\n        std::size_t count = 0u;\n        bool changed = false;\n\n        {\n            std::lock_guard<std::mutex> lock(g_mutex);\n            for (auto it = g_cache.begin();\n                 it != g_cache.end() && count < dead.size();) {\n                if (it->second.device == device) {\n                    dead[count++] = it->second;\n                    it = g_cache.erase(it);\n                    changed = true;\n                } else {\n                    ++it;\n                }\n            }\n\n            if (count == 0u) {\n                // The final batch also removes lifetime quarantine records.\n                for (auto it = g_ambiguous_view_device.begin();\n                     it != g_ambiguous_view_device.end();) {\n                    if (it->second == device) {\n                        it = g_ambiguous_view_device.erase(it);\n                        changed = true;\n                    } else {\n                        ++it;\n                    }\n                }\n            }\n\n            // Invalidate before releasing any native SRV. TLS may retain an\n            // owned AddRef until its next lookup, never an unowned pointer.\n            if (changed)\n                invalidate_all_companion_epochs();\n        }\n\n        // COM Release may re-enter resource-view callbacks.\n        for (std::size_t n = 0u; n < count; ++n)\n            release_set(dead[n]);\n\n        if (count == 0u)\n            break;\n    }\n}\n\n"

ADMISSION_OLD = "    companion_set old{};\n    bool had_old = false;\n    bool accepted = false;\n\n    {\n        std::lock_guard<std::mutex> lock(\n            g_mutex);\n\n        if (g_ambiguous_view_device.find(key) ==\n            g_ambiguous_view_device.end()) {\n            const auto found =\n                g_cache.find(key);\n\n            if (found != g_cache.end() &&\n                found->second.logical_hash != logical_hash) {\n                // The native handle is still live but now claims a different\n                // exact logical identity. Neither association is safe. Remove\n                // the previous record and quarantine this handle until its\n                // destroy_resource_view callback.\n                old = found->second;\n                g_cache.erase(found);\n                g_ambiguous_view_device.emplace(\n                    key,\n                    native_device);\n                had_old = true;\n            } else if (found != g_cache.end()) {\n                old = found->second;\n                found->second = set;\n                had_old = true;\n                accepted = true;\n            } else {\n                g_cache.emplace(\n                    key,\n                    set);\n                accepted = true;\n            }\n\n            invalidate_companion_epoch(key);\n        }\n    }\n\n"
ADMISSION_NEW = "    companion_set old{};\n    bool had_old = false;\n    bool accepted = false;\n\n    try {\n        std::lock_guard<std::mutex> lock(\n            g_mutex);\n\n        if (g_ambiguous_view_device.find(key) ==\n            g_ambiguous_view_device.end()) {\n            const auto found =\n                g_cache.find(key);\n\n            if (found != g_cache.end() &&\n                found->second.logical_hash != logical_hash) {\n                // A conflicting live native SRV must never acquire a new\n                // texture identity. Quarantine it through the destroy hook.\n                old = found->second;\n                g_cache.erase(found);\n                had_old = true;\n                g_ambiguous_view_device.emplace(\n                    key,\n                    native_device);\n            } else if (found != g_cache.end()) {\n                old = found->second;\n                found->second = set;\n                had_old = true;\n                accepted = true;\n            } else {\n                g_cache.emplace(\n                    key,\n                    set);\n                accepted = true;\n            }\n\n            invalidate_companion_epoch(key);\n        }\n    } catch (...) {\n        // unordered_map::emplace can allocate and throw after a previous\n        // record was detached. The callback must not unwind into ReShade,\n        // leak COM references, or expose a stale TLS identity after failure.\n        // An uncertain cache state is globally fail-open to stock DSR.\n        g_quarantined.store(\n            true, std::memory_order_release);\n        invalidate_all_companion_epochs();\n        if (had_old)\n            release_set(old);\n        release_set(set);\n        ++g_fail_open;\n        return;\n    }\n\n"


def transform(source: str) -> str:
    for name, old, new in (
        ("teardown", RELEASE_OLD, RELEASE_NEW),
        ("admission", ADMISSION_OLD, ADMISSION_NEW),
    ):
        if source.count(new) == 1 and source.count(old) == 0:
            continue
        if source.count(old) != 1 or source.count(new) != 0:
            raise RuntimeError("V231 unsafe source drift: " + name)
        source = source.replace(old, new, 1)
    return source


def run(apply: bool) -> None:
    original = TARGET.read_text(encoding="utf-8").replace("\r\n", "\n")
    generated = transform(original)
    if apply:
        TARGET.write_text(generated, encoding="utf-8", newline="")
    elif original != generated:
        raise RuntimeError("V231 materialization has not been applied")
    print("V231_RESOURCE_EXCEPTION_SAFETY_" + ("APPLIED" if apply else "VERIFIED"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    run(parser.parse_args().apply)
