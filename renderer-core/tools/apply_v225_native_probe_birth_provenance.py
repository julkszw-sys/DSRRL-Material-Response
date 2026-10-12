#!/usr/bin/env python3
"""V225 opt-in native EnvSpec resource creation/registration provenance.

V224 runtime: one exact BC6H 256x256x6 t12 resource with valid typed cube SRV
misses BOTH the registered view and the underlying resource (12/12). Neither
the probe ordinal nor asset identity is known. Repeated blind SRV joins cannot
repair a resource that never entered the SHA-256 registry.

This generated-source-only overlay records *why* a geometrically matching
native resource did not register. It does not substitute, hash readback, replay,
or mutate resources and does not change a draw outcome. P_Metal untouched.

Birth stages:
0 = NOT_SEEN_BY_INIT_RESOURCE (or failed descriptor/device gate)
1 = CREATE_NO_INITIAL_DATA (future upload/copy topology remains OPEN)
2 = CPU_DATA_BUT_TIGHT_BC6H_EXTRACTION_FAILED
3 = CPU_SHA256_NOT_IN_NATIVE_PROBE_WHITELIST
4 = CPU_SHA256_MATCHES_EXACT_NATIVE_PROBE

The complete 64-character CPU SHA is logged only for already captured initial
data, allowing exact offline asset matching without a speculative probe ID.
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "renderer-core/include/dsrrl/runtime/envspec_resource_runtime.hpp"
RESOURCE = ROOT / "renderer-core/src/runtime/envspec_resource_runtime.cpp"
DRAW = ROOT / "renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp"

HEADER_ANCHOR = "        bool resource_registered = false;\n"
HEADER_INSERT = """        // V225 only: diagnostics, no new probe authority.
#if defined(DSRRL_V225_NATIVE_PROBE_BIRTH_PROVENANCE)
        std::uint32_t birth_stage = 0u;
        char birth_sha256[65]{};
#endif
"""

GLOBAL_ANCHOR = "    g_resource_by_view;\n"
GLOBAL_INSERT = """#if defined(DSRRL_V225_NATIVE_PROBE_BIRTH_PROVENANCE)
// Same exact BC6H cubemap descriptor domain as g_native_resources.
// Records are keyed by native resource handle and invalidated on destroy.
// No new SHA->probe mapping or resource lifetime retention.
struct v225_native_birth {
    std::uint32_t stage = 0u;
    char sha256[65]{};
};
std::unordered_map<std::uint64_t, v225_native_birth> g_v225_birth;

void v225_record_birth(
    std::uint64_t key,
    std::uint32_t stage,
    const hashing::sha256_digest *digest) noexcept
{
    v225_native_birth state{};
    state.stage = stage;
    if (digest != nullptr) {
        constexpr char h[] = "0123456789abcdef";
        for (std::size_t i = 0u; i < digest->size(); ++i) {
            state.sha256[2u * i] = h[(*digest)[i] >> 4u];
            state.sha256[2u * i + 1u] = h[(*digest)[i] & 0x0fu];
        }
        state.sha256[64] = '\\0';
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_v225_birth[key] = state;
}
#endif

"""

INIT_ANCHOR = """    ++g_native_candidates;

    std::vector<std::uint8_t> tight;"""
INIT_REPLACE = """    ++g_native_candidates;
#if defined(DSRRL_V225_NATIVE_PROBE_BIRTH_PROVENANCE)
    v225_record_birth(
        resource_handle.handle,
        initial_data == nullptr ? 1u : 2u,
        nullptr);
#endif

    std::vector<std::uint8_t> tight;"""

DIGEST_ANCHOR = """    const auto digest =
        hashing::sha256(
            tight.data(),
            tight.size());

    const auto probe ="""
DIGEST_REPLACE = """    const auto digest =
        hashing::sha256(
            tight.data(),
            tight.size());
#if defined(DSRRL_V225_NATIVE_PROBE_BIRTH_PROVENANCE)
    v225_record_birth(resource_handle.handle, 3u, &digest);
#endif

    const auto probe ="""

HASH_MATCH_ANCHOR = """    if (!probe.has_value()) {
        ++g_native_hash_miss;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);
        g_native_resources["""
HASH_MATCH_REPLACE = """    if (!probe.has_value()) {
        ++g_native_hash_miss;
        return;
    }
#if defined(DSRRL_V225_NATIVE_PROBE_BIRTH_PROVENANCE)
    v225_record_birth(resource_handle.handle, 4u, &digest);
#endif

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);
        g_native_resources["""

DESTROY_ANCHOR = """    g_native_resources.erase(
        resource_handle.handle);
"""
DESTROY_REPLACE = DESTROY_ANCHOR + """#if defined(DSRRL_V225_NATIVE_PROBE_BIRTH_PROVENANCE)
    g_v225_birth.erase(resource_handle.handle);
#endif
"""

RESET_ANCHOR = """        g_native_resources.clear();
"""
RESET_REPLACE = RESET_ANCHOR + """#if defined(DSRRL_V225_NATIVE_PROBE_BIRTH_PROVENANCE)
        g_v225_birth.clear();
#endif
"""

JOIN_ANCHOR = """        observed.resource_registered =
            by_resource != g_native_resources.end();

        if (!observed.resource_registered)"""
JOIN_REPLACE = """        observed.resource_registered =
            by_resource != g_native_resources.end();
#if defined(DSRRL_V225_NATIVE_PROBE_BIRTH_PROVENANCE)
        const auto birth = g_v225_birth.find(observed.resource_key);
        if (birth != g_v225_birth.end()) {
            observed.birth_stage = birth->second.stage;
            std::memcpy(
                observed.birth_sha256, birth->second.sha256,
                sizeof(observed.birth_sha256));
        }
#endif

        if (!observed.resource_registered)"""

DRAW_ANCHOR = """                recovered ? "resource_join_ready" : "stock_dsr_fail_open");
            reshade::log::message(reshade::log::level::info, line);
"""
DRAW_REPLACE = """                recovered ? "resource_join_ready" : "stock_dsr_fail_open");
            reshade::log::message(reshade::log::level::info, line);
#if defined(DSRRL_V225_NATIVE_PROBE_BIRTH_PROVENANCE)
            const auto stats = env_resources_.telemetry();
            char origin[580]{};
            std::snprintf(
                origin, sizeof(origin),
                "[DSRRL V225 PROBE ORIGIN] "
                "rx=%u route=%u slot=%u "
                "resource_key=%llx birth_stage=%u birth_sha256=%s "
                "registered_resource=%u registered_view=%u "
                "native_candidates=%llu native_matches=%llu "
                "native_hash_miss=%llu native_views=%llu "
                "no_probe_guess=1 stock_dsr_if_unresolved=1 pixel=OPEN",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(material.route_index),
                static_cast<unsigned>(material.material_slot),
                static_cast<unsigned long long>(resource_diag.a.resource_key),
                resource_diag.a.birth_stage,
                resource_diag.a.birth_sha256[0] != '\\0'
                    ? resource_diag.a.birth_sha256 : "NO_CPU_SHA",
                resource_diag.a.resource_registered ? 1u : 0u,
                resource_diag.a.view_registered ? 1u : 0u,
                static_cast<unsigned long long>(stats.native_candidates),
                static_cast<unsigned long long>(stats.native_matches),
                static_cast<unsigned long long>(stats.native_hash_miss),
                static_cast<unsigned long long>(stats.view_matches));
            reshade::log::message(reshade::log::level::info, origin);
#endif
"""

def replace_once(text: str, old: str, new: str, location: str) -> str:
    if text.count(new) == 1:
        return text
    if text.count(old) != 1:
        raise RuntimeError(f"V225 drift {location}: {old[:90]!r}")
    return text.replace(old, new, 1)

def patch(h: str, r: str, d: str) -> tuple[str, str, str]:
    if "native_resource_endpoint_v224" not in h:
        raise RuntimeError("V225 requires authenticated V224 source")
    if "prepare_exact_native_resource_join_v224" not in r:
        raise RuntimeError("V225 requires materialized V224 resource join")
    if "[DSRRL V224 NATIVE RESOURCE JOIN]" not in d:
        raise RuntimeError("V225 requires V224 draw cut")
    h = replace_once(h, HEADER_ANCHOR, HEADER_INSERT + HEADER_ANCHOR,
                     "v224 diagnostic ABI")
    r = replace_once(r, GLOBAL_ANCHOR, GLOBAL_ANCHOR + GLOBAL_INSERT,
                     "birth registry")
    r = replace_once(r, INIT_ANCHOR, INIT_REPLACE, "creation callback")
    r = replace_once(r, DIGEST_ANCHOR, DIGEST_REPLACE, "CPU digest")
    r = replace_once(r, HASH_MATCH_ANCHOR, HASH_MATCH_REPLACE,
                     "known SHA registration")
    r = replace_once(r, DESTROY_ANCHOR, DESTROY_REPLACE,
                     "resource destruction")
    r = replace_once(r, RESET_ANCHOR, RESET_REPLACE, "device reset")
    r = replace_once(r, JOIN_ANCHOR, JOIN_REPLACE, "draw resource identity")
    d = replace_once(d, DRAW_ANCHOR, DRAW_REPLACE, "bounded draw logger")
    if "get_ptde_cube(diag.a.probe, slot, ignored)" not in r:
        raise RuntimeError("V225 must preserve V224 authorized source lookup")
    return h, r, d

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    opts = ap.parse_args()
    files = (HEADER, RESOURCE, DRAW)
    old = tuple(p.read_text(encoding="utf-8").replace("\r\n", "\n")
                for p in files)
    new = patch(*old)
    if opts.apply:
        for p, before, after in zip(files, old, new):
            if before != after:
                p.write_text(after, encoding="utf-8", newline="\n")
    elif old != new:
        raise RuntimeError("V225 exact probe birth generated source not applied")
    print("V225_PROBE_BIRTH_PROVENANCE_" +
          ("APPLIED" if opts.apply else "VERIFIED"))
    print("V225_NO_NEW_PROBE_MAPPING_NO_GPU_READBACK_PMETAL_UNCHANGED")
    print("V225_RUNTIME_AND_PIXEL_OPEN")

if __name__ == "__main__":
    main()
