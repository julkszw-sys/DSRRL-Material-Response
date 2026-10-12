#!/usr/bin/env python3
"""V231: materialize exact live PTDE DDS SRV reuse into generated source.

Source donor: original V229/V230 renderer (resource hook unchanged). V230
caches immutable game root, drops redundant file metadata check; V231 adds
strictly scoped live SRV sharing without caching past original stock-view
lifetime. Never changes shader, Lerp, P_Metal, native hooks or resource routing.

Only one successful source postcondition authorizes native build. The binary
must contain [DSRRL V231 LIVE SIDECAR REUSE] marker so source-only unit tests
cannot masquerade as runtime integration.
"""
import argparse
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/"renderer-core/src/runtime/material_resource_draw_runtime.cpp"
MARKER="[DSRRL V231 LIVE SIDECAR REUSE]"
CHANGES=[
(
'#include "dsrrl/runtime/companion_tls_way_selector.hpp"\n',
'#include "dsrrl/runtime/companion_tls_way_selector.hpp"\n#include "dsrrl/runtime/live_sidecar_reuse_policy_v230.hpp"\n'
),
(
'''struct companion_set {
    ID3D11Device *device = nullptr;
    std::uint64_t logical_hash = 0;
    ID3D11ShaderResourceView *specular = nullptr;''',
'''struct companion_set {
    ID3D11Device *device = nullptr;
    std::uint64_t logical_hash = 0;
    // Exact name is required before reusing any view. FNV64 only narrows
    // lookup; it never authorizes ambiguous/mismatched texture identity.
    std::wstring exact_logical_name{};
    ID3D11ShaderResourceView *specular = nullptr;'''
),
(
'''std::unordered_map<std::uint64_t, companion_set> g_cache;
// A live native SRV handle''',
'''std::unordered_map<std::uint64_t, companion_set> g_cache;
// Secondary lookup contains no owned COM references. Every entry is tied
// to a live stock SRV and is removed on view destruction / device reset.
std::unordered_multimap<std::uint64_t,std::uint64_t>
    g_live_by_logical_hash;
std::atomic<std::uint64_t> g_live_companion_reuses{0u};
// A live native SRV handle'''
),
(
'''    set.logical_hash = 0u;
}

void release_cache() noexcept''',
'''    set.logical_hash = 0u;
    set.exact_logical_name.clear();
}

// Called only with g_mutex owned; do not Release COM inside this mutex.
void erase_live_companion_index(
    std::uint64_t stock_key,
    std::uint64_t logical_hash) noexcept
{
    auto span =
        g_live_by_logical_hash.equal_range(
            logical_hash);
    for (auto it = span.first;
         it != span.second;) {
        if (it->second == stock_key)
            it = g_live_by_logical_hash.erase(it);
        else
            ++it;
    }
}

void release_cache() noexcept'''
),
(
'''        dead.swap(g_cache);
        g_ambiguous_view_device.clear();''',
'''        dead.swap(g_cache);
        g_live_by_logical_hash.clear();
        g_ambiguous_view_device.clear();'''
),
(
'''            if (it->second.device == device) {
                dead.push_back(it->second);
                it = g_cache.erase(it);''',
'''            if (it->second.device == device) {
                erase_live_companion_index(
                    it->first,
                    it->second.logical_hash);
                dead.push_back(it->second);
                it = g_cache.erase(it);'''
),
(
'''    companion_set set{};
    set.device = native_device;
    set.device->AddRef();
    set.logical_hash =
        logical_hash;''',
'''    companion_set set{};
    set.device = native_device;
    set.device->AddRef();
    set.logical_hash = logical_hash;
    try {
        set.exact_logical_name = logical_name;
    } catch (...) {
        release_set(set);
        return;
    }

    const bool need_diffuse =
        generated::diffuse_target_hash_allowed_v12(
            logical_hash);
    const bool need_normal =
        generated::normal_target_hash_allowed_v12(
            logical_hash);
    bool reused = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto span =
            g_live_by_logical_hash.equal_range(
                logical_hash);
        for (auto it = span.first;
             it != span.second;
             ++it) {
            const auto found = g_cache.find(it->second);
            if (found == g_cache.end())
                continue;
            const auto &candidate = found->second;
            if (!may_reuse_live_sidecar_v230(
                    {
                        reinterpret_cast<std::uintptr_t>(candidate.device),
                        candidate.exact_logical_name,
                        candidate.specular != nullptr,
                        candidate.diffuse != nullptr,
                        candidate.normal != nullptr
                    },
                    {
                        reinterpret_cast<std::uintptr_t>(native_device),
                        logical_name,
                        spec_member,
                        need_diffuse,
                        need_normal
                    }))
                continue;
            if (spec_member) {
                set.specular = candidate.specular;
                set.specular->AddRef();
            }
            if (need_diffuse) {
                set.diffuse = candidate.diffuse;
                set.diffuse->AddRef();
            }
            if (need_normal) {
                set.normal = candidate.normal;
                set.normal->AddRef();
            }
            reused = true;
            break;
        }
    }
    if (reused) {
        g_live_companion_reuses.fetch_add(
            1u, std::memory_order_relaxed);
        if (spec_member)
            account_load(load_status::ready);
        if (need_diffuse)
            account_load(load_status::ready);
        if (need_normal)
            account_load(load_status::ready);
    } else {
        // The original generated full-1295 instrumentation, SHA checks,
        // D3D11 creation, and material routes stay byte-identical below.
'''),
(
'''    const auto key =
        static_cast<std::uint64_t>(
            view.handle);''',
'''    }

    const auto key =
        static_cast<std::uint64_t>(
            view.handle);''')(
'''    bool had_old = false;
    bool accepted = false;

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);

        if (g_ambiguous_view_device.find(key) ==
            g_ambiguous_view_device.end()) {
            const auto found =
                g_cache.find(key);

            if (found != g_cache.end() &&
                found->second.logical_hash != logical_hash) {
                // The native handle is still live but now claims a different
                // exact logical identity. Neither association is safe. Remove
                // the previous record and quarantine this handle until its
                // destroy_resource_view callback.
                old = found->second;
                g_cache.erase(found);
                g_ambiguous_view_device.emplace(
                    key,
                    native_device);
                had_old = true;
            } else if (found != g_cache.end()) {
                old = found->second;
                found->second = set;
                had_old = true;
                accepted = true;
            } else {
                g_cache.emplace(
                    key,
                    set);
                accepted = true;
            }

            invalidate_companion_epoch(key);
        }
    }''',
'''    bool had_old = false;
    bool accepted = false;
    bool new_record = false;

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);

        if (g_ambiguous_view_device.find(key) ==
            g_ambiguous_view_device.end()) {
            const auto found =
                g_cache.find(key);

            if (found != g_cache.end() &&
                (found->second.logical_hash != logical_hash ||
                 found->second.exact_logical_name != logical_name)) {
                // A recycled stock handle must never resolve to a
                // companion of another exact name, even on FNV collision.
                old = found->second;
                erase_live_companion_index(
                    key, old.logical_hash);
                g_cache.erase(found);
                g_ambiguous_view_device.emplace(
                    key, native_device);
                had_old = true;
            } else if (found != g_cache.end()) {
                old = found->second;
                found->second = set;
                had_old = true;
                accepted = true;
            } else {
                g_cache.emplace(key, set);
                accepted = true;
                new_record = true;
            }

            // The index is an optional speedup; allocation failure must
            // never block an otherwise exact stock->PTDE resource route.
            if (new_record) {
                try {
                    g_live_by_logical_hash.emplace(
                        logical_hash, key);
                } catch (...) {
                    // Keep authoritative g_cache; no secondary index.
                }
            }
            invalidate_companion_epoch(key);
        }
    }'''
),
(
'''        if (it != g_cache.end()) {
            dead = it->second;
            g_cache.erase(it);
            found = true;
            changed = true;
        }''',
'''        if (it != g_cache.end()) {
            dead = it->second;
            erase_live_companion_index(
                key, dead.logical_hash);
            g_cache.erase(it);
            found = true;
            changed = true;
        }'''
),
(
'''    g_named_views.store(0u);
    g_sidecar_ready.store(0u);''',
'''    g_named_views.store(0u);
    g_live_companion_reuses.store(0u);
    g_sidecar_ready.store(0u);'''
)
]

def apply(src: str)->str:
    if MARKER in src:
        return src
    # V230 immutable path source should already have been applied before this.
    if 'const std::filesystem::path &process_dir() noexcept' not in src:
        raise ValueError("V230 process-root overlay absent before V231")
    if 'std::filesystem::is_regular_file(path)' in src:
        raise ValueError("V230 redundant cold file I/O still present")
    for old,new in CHANGES:
        count=src.count(old)
        if count != 1:
            raise ValueError("V231 source anchor count="+str(count)+" "+repr(old[:85]))
        src=src.replace(old,new,1)
    anchor='    ++g_named_views;\n}\n\nvoid on_destroy_resource_view('
    if src.count(anchor)!=1:
        raise ValueError("V231 marker anchor mismatch")
    src=src.replace(anchor,'    // '+MARKER+' exact reused view or unchanged cold path\\n'+anchor,1)
    return src

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--apply",action="store_true")
    x=ap.parse_args()
    old=SOURCE.read_text(encoding="utf-8").replace("\r\n","\n")
    new=apply(old)
    if x.apply and old!=new:
        SOURCE.write_text(new,encoding="utf-8",newline="\n")
    elif not x.apply and old!=new:
        raise SystemExit("V231 absent from generated source")
    print("V231_LIVE_PTDE_COMPANION_SRV_REUSE_"+("APPLIED" if x.apply else "VERIFIED"))
    print("V231_PROTECT_LAST_TEXTURE_NAME_RESOURCE_HOOK_SHADER_PIXEL_UNCHANGED")

if __name__=="__main__":
    main()
