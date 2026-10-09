#!/usr/bin/env python3
"""Materialize map-scoped P_Metal selector producer on exact R43 RC1 source."""
from pathlib import Path
p=Path("renderer-core/src/runtime/pmetal_env_source_runtime.cpp")
s=p.read_text()
def replace(a,b):
 global s
 assert s.count(a)==1,(a[:80],s.count(a))
 s=s.replace(a,b,1)
replace('#include "dsrrl/runtime/pmetal_producer_state.hpp"',
        '#include "dsrrl/runtime/pmetal_producer_state.hpp"\n#include "dsrrl/runtime/pmetal_map_source.hpp"')
old="""    if (!safe_read(
            static_cast<const std::uint8_t *>(
                selector_stack) +
                parent_offset,
            parent_return) ||
        parent_return !=
            g_source_base +
                k_parent_return)
        return;
"""
new="""    const bool exact_parent =
        safe_read(
            static_cast<const std::uint8_t *>(
                selector_stack) +
                parent_offset,
            parent_return) &&
        parent_return ==
            g_source_base +
                k_parent_return;
#if !defined(DSRRL_PMETAL_R43_MAP_SOURCE_JOIN)
    if (!exact_parent)
        return;
#endif
"""
replace(old,new)
replace("""    if (telemetry::effect_enabled())
        g_parent_gate_ok.store(
            true,""",
"""    if (exact_parent && telemetry::effect_enabled())
        g_parent_gate_ok.store(
            true,""")
needle="""    pmetal_envspec_source next{};

    // R41 producer-driven shadow join."""
injected="""    // High selector byte chooses the source area. The material/owner/source
    // invariants below remain mandatory independently of stack parent ABI.
    const unsigned selector_map =
        10u + ((static_cast<unsigned>(
                    static_cast<std::uint16_t>(endpoints.a)) >> 8u) & 0x7fu);
    const bool map_blend_compatible =
        endpoints.beta == 0.0f ||
        (((static_cast<unsigned>(
             static_cast<std::uint16_t>(endpoints.b)) >> 8u) & 0x7fu)
         == selector_map - 10u);
    pmetal_envspec_source next{};

    // R41 producer-driven shadow join."""
replace(needle,injected)
old="""        next.serial = epoch;
        pmetal_producer_state_clear();"""
new="""        next.serial = epoch;
#if defined(DSRRL_PMETAL_R43_MAP_SOURCE_JOIN)
        if (map_blend_compatible)
            (void)pmetal_map_source::publish(
                material,next,selector_map);
#endif
        pmetal_producer_state_clear();"""
replace(old,new)
old="""    next.serial = epoch;
    pmetal_producer_state_clear();"""
new="""    next.serial = epoch;
#if defined(DSRRL_PMETAL_R43_MAP_SOURCE_JOIN)
    if (map_blend_compatible)
        (void)pmetal_map_source::publish(
            material,next,selector_map);
#endif
    pmetal_producer_state_clear();"""
replace(old,new)
replace("""    g_source_cache_generation.fetch_add(
        1u,
        std::memory_order_relaxed);
}""",
"""    g_source_cache_generation.fetch_add(
        1u,
        std::memory_order_relaxed);
#if defined(DSRRL_PMETAL_R43_MAP_SOURCE_JOIN)
    pmetal_map_source::invalidate();
#endif
}""")
p.write_text(s)
print("PASS R43 map-scoped producer source patch")
