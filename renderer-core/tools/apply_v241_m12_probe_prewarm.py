#!/usr/bin/env python3
"""V241 A/B cold PTDE probe GPU materialization before first area traversal.

This is NOT a production preload policy. Probe IDs 130->40 came from the
owner's exact m12 runtime HemEnvLerp witness on V239. Materialize only their
three verified PTDE EnvSpec slots and independent EnvDiffuse cubes at
init_device after hash-authenticated packs and sampler are ready. No source,
selector, resource identity, pixel shader, MR or PTDE math changes.
"""
from pathlib import Path
import argparse

PATH=Path("renderer-core/src/runtime/envspec_resource_runtime.cpp")
ANCHOR_INCLUDE="#include <cstdint>\n"
NEW_INCLUDE="#include <cstdint>\n#include <cstdio>\n"
ANCHOR_DECL="""void on_init_device(
    device *device_ptr)
{
"""
INSERT_DECL="""// V241 experimental preload of exact owner-observed m12 pair only.
// Declarations before the init event are necessary: implementations below.
bool get_ptde_cube(
    std::uint16_t probe,
    std::uint8_t slot,
    resource_view &view) noexcept;
bool get_ptde_envdiffuse_cube(
    std::uint16_t probe,
    resource_view &view) noexcept;

"""+ANCHOR_DECL
ANCHOR_BODY="""    if (pack_ready)
        ++g_pack_admit_ok;
    else
        ++g_pack_admit_fail;
"""
INSERT_BODY="""    // V241 A/B source-verified m12 transition (130 -> 40). Do the same
    // materialization that prepare_bound() would do on a cold RX34 draw,
    // but during device initialization, outside g_mutex. Only 8 cubes:
    // 2 probes x 3 EnvSpec MTD slots + 2 separate EnvDiffuse cubes.
    // Failed operations do not disable any bridge; normal lazy admission
    // and existing fail-open paths remain intact.
    unsigned warm_spec = 0u;
    unsigned warm_diffuse = 0u;
    if (pack_ready && sampler_ready) {
        constexpr std::uint16_t k_m12_pair[] = {130u, 40u};
        for (const auto probe : k_m12_pair) {
            for (std::uint8_t slot = 0u; slot < 3u; ++slot) {
                resource_view ignored{};
                if (get_ptde_cube(probe, slot, ignored))
                    ++warm_spec;
            }
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
            if (envdiffuse_pack_ready) {
                resource_view ignored{};
                if (get_ptde_envdiffuse_cube(probe, ignored))
                    ++warm_diffuse;
            }
#endif
        }
    }
    char warm_line[256]{};
    std::snprintf(warm_line, sizeof(warm_line),
        "[DSRRL V241 PROBE PREWARM AB] scope=m12_probe_130_40 "
        "phase=INIT_DEVICE_ONLY spec_created=%u/6 diffuse_created=%u/2 "
        "pack=%u sampler=%u fallback=STOCK_IF_NOT_READY "
        "ptde_operator=UNCHANGED runtime_hitch=OPEN",
        warm_spec, warm_diffuse,
        pack_ready ? 1u : 0u, sampler_ready ? 1u : 0u);
    reshade::log::message(reshade::log::level::info, warm_line);

"""+ANCHOR_BODY
def patch(source: str, apply: bool=False)->str:
    old=(source.count(ANCHOR_INCLUDE)==1 and NEW_INCLUDE not in source and source.count(ANCHOR_DECL)==1 and source.count(ANCHOR_BODY)==1
         and INSERT_BODY not in source and INSERT_DECL not in source)
    new=(source.count(NEW_INCLUDE)==1 and source.count(INSERT_DECL)==1 and source.count(INSERT_BODY)==1)
    if old==new: raise ValueError("V241 source anchors mismatched")
    for token in ("bool get_ptde_cube(", "bool get_ptde_envdiffuse_cube(",
                  "g_envdiffuse_pack_ready", "g_ptde_cubes.emplace(",
                  "g_ptde_envdiffuse_cubes.emplace(",
                  "g_snapshot_epoch.fetch_add(", "if (!ready && needs_cube) {"):
        if token not in source: raise ValueError("V241 guard missing "+token)
    if not apply or new: return source
    return source.replace(ANCHOR_INCLUDE,NEW_INCLUDE,1).replace(ANCHOR_DECL,INSERT_DECL,1).replace(ANCHOR_BODY,INSERT_BODY,1)
if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--apply",action="store_true")
    a=parser.parse_args()
    old=PATH.read_text(encoding="utf-8")
    out=patch(old,a.apply)
    if out!=old: PATH.write_text(out,encoding="utf-8")
    print("V241_EXACT_130_40_EARLY_RESOURCE_CREATE_SOURCE_GUARD_PASS")
