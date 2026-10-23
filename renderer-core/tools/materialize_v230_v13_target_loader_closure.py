#!/usr/bin/env python3
"""V230 V13 target loader closure: source-certified exact names only.

The V13 armor supplement extends source name membership, preload targeting,
and pair/triple predicates, but on_init_resource_view still uses V12-only
target hashes when acquiring actual Diffuse/Normal DDS SRVs. That makes the
new HD_M_9360 target names reach the callback but never load their DDS.
This narrowly closes that inconsistent target-membership callsite.
Fail open for a missing/bad DDS; never manufacture or guess content.
"""
from pathlib import Path
p=Path("renderer-core/src/runtime/material_resource_draw_runtime.cpp")
s=p.read_text("utf-8")
def one(a,b):
    global s
    n=s.count(a)
    if n!=1: raise RuntimeError(f"V13_TARGET_LOADER_DRIFT count={n} anchor={a[:65]!r}")
    s=s.replace(a,b,1)

one('''    if (generated::
        diffuse_target_hash_allowed_v12(
            logical_hash)) {''',
'''    if (generated::
            diffuse_target_hash_allowed_v12(logical_hash) ||
        armor_v13::diffuse_name(logical_hash)) {''')

one('''    if (generated::
        normal_target_hash_allowed_v12(
            logical_hash)) {''',
'''    if (generated::
            normal_target_hash_allowed_v12(logical_hash) ||
        armor_v13::normal_name(logical_hash)) {''')

# Bounded witness for physical outcome on source-verified V13 new targets.
# No per-frame overhead: runs at name-carrying SRV construction only.
one('''    // std::wstring move is noexcept: do not introduce allocations or''',
'''#if defined(DSRRL_V230_V13_TARGET_LOADER_CLOSURE)
    if (armor_v13::diffuse_name(logical_hash) ||
        armor_v13::normal_name(logical_hash)) {
        static std::atomic<std::uint32_t> logged{0u};
        const auto n=logged.fetch_add(1u,std::memory_order_relaxed)+1u;
        if(n<=80u) {
            char b[240]{};
            std::snprintf(b,sizeof(b),
                "[DSRRL V13 LOADER CUT] n=%u hash=%016llx diffuse_target=%u normal_target=%u loaded_diff=%u loaded_norm=%u",
                n,static_cast<unsigned long long>(logical_hash),
                armor_v13::diffuse_name(logical_hash)?1u:0u,
                armor_v13::normal_name(logical_hash)?1u:0u,
                set.diffuse?1u:0u,set.normal?1u:0u);
            reshade::log::message(reshade::log::level::warning,b);
        }
    }
#endif
    // std::wstring move is noexcept: do not introduce allocations or''')

p.write_text(s,"utf-8")
print("V13_TARGET_LOADER_CLOSURE_PASS actual DDS acquisition aligned to source-certified V13 hash tables")
