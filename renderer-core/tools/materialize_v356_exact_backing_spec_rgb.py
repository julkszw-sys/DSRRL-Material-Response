#!/usr/bin/env python3
"""V356: exact SAME D3D11 backing resource + SRV description alias.
Only salvage a known PTDE texture sidecar when a second native SRV points
to the exact already witnessed resource on same device, view description matches,
and ALL extant names agree. FLVER/MTD/receiver gates remain mandatory.
Apply after V323 source-provenance and V354 materializer. Fail open otherwise.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
path=root/"src/runtime/material_resource_draw_runtime.cpp"
s=path.read_text(encoding="utf8")

def replace(old,new,label):
 global s
 n=s.count(old)
 if n!=1: raise RuntimeError(f"V356 source anchor {label}: {n}")
 s=s.replace(old,new,1)

replace('std::unordered_map<std::uint64_t, companion_set> g_cache;',
'''std::unordered_map<std::uint64_t, companion_set> g_cache;

#if defined(DSRRL_V356_EXACT_BACKING_SRV_ALIAS)
// Lifetime scope = an active, original retail resource view already certified
// by the exact logical-name transport. No resource hashing, name guessing,
// cross-view approximation, FLVER fallback, or content-copy inference.
struct v356_backing_witness {
    std::uint64_t view_key = 0u;
    std::uint64_t logical_hash = 0u;
    ID3D11Device *device = nullptr; // non-owning; device cleanup erases first
    D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
};
std::unordered_map<std::uintptr_t,std::vector<v356_backing_witness>>
    g_v356_backing_witnesses;
std::unordered_map<std::uint64_t,std::uintptr_t>
    g_v356_backing_by_view;
std::atomic<std::uint64_t> g_v356_exact_alias_hits{0u};
std::atomic<std::uint64_t> g_v356_alias_ambiguous{0u};
std::atomic<std::uint64_t> g_v356_alias_desc_reject{0u};
#endif''',"state")

replace('''        g_ambiguous_view_device.clear();
    }

    for (auto &entry : dead)''',
'''        g_ambiguous_view_device.clear();
#if defined(DSRRL_V356_EXACT_BACKING_SRV_ALIAS)
        g_v356_backing_witnesses.clear();
        g_v356_backing_by_view.clear();
#endif
    }

    for (auto &entry : dead)''',"all cleanup")

replace('''        if (changed)
            g_view_cache_journal.publish(0u);
    }

    for (auto &set : dead)''',
'''#if defined(DSRRL_V356_EXACT_BACKING_SRV_ALIAS)
        for (auto it = g_v356_backing_witnesses.begin();
             it != g_v356_backing_witnesses.end();) {
            auto &witnesses = it->second;
            for (const auto &w : witnesses)
                if (w.device == device)
                    g_v356_backing_by_view.erase(w.view_key);
            witnesses.erase(
                std::remove_if(witnesses.begin(),witnesses.end(),
                    [device](const auto &w){return w.device==device;}),
                witnesses.end());
            if (witnesses.empty())
                it=g_v356_backing_witnesses.erase(it);
            else ++it;
        }
#endif
        if (changed)
            g_view_cache_journal.publish(0u);
    }

    for (auto &set : dead)''',"device cleanup")

needle='''bool snapshot_companion_cached(
    ID3D11ShaderResourceView *stock,'''
helper=r'''#if defined(DSRRL_V356_EXACT_BACKING_SRV_ALIAS)
bool v356_same_exact_view(
    const D3D11_SHADER_RESOURCE_VIEW_DESC &a,
    const D3D11_SHADER_RESOURCE_VIEW_DESC &b) noexcept
{
    // Both descriptors are zero initialized before GetDesc(). Requiring a
    // byte-identical typed descriptor is deliberately stricter than guessing
    // equivalence between mip ranges, array slices or cube faces.
    return std::memcmp(&a,&b,sizeof(a))==0;
}

void v356_unbind_named_view(std::uint64_t view_key) noexcept
{
    const auto owner=g_v356_backing_by_view.find(view_key);
    if (owner==g_v356_backing_by_view.end()) return;
    const auto backing=owner->second;
    const auto it=g_v356_backing_witnesses.find(backing);
    if (it!=g_v356_backing_witnesses.end()) {
        auto &list=it->second;
        list.erase(std::remove_if(list.begin(),list.end(),
              [view_key](const auto &w){return w.view_key==view_key;}),
              list.end());
        if (list.empty()) g_v356_backing_witnesses.erase(it);
    }
    g_v356_backing_by_view.erase(owner);
}

// Native COM SRV->GetResource is the exact transport; this path does not
// create an identity for an unknown backing. It can only reuse a live,
// independently named and hash-certified view of the SAME physical object.
bool v356_snapshot_exact_backing(
    ID3D11ShaderResourceView *stock,
    asset_class cls,
    std::uint64_t &hash,
    ID3D11ShaderResourceView *&view,
    bool retain_view) noexcept
{
    if (stock==nullptr) return false;
    ID3D11Resource *backing=nullptr;
    stock->GetResource(&backing);
    if (backing==nullptr) return false;
    ID3D11Device *device=nullptr;
    stock->GetDevice(&device);
    if (device==nullptr) {backing->Release();return false;}
    D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
    stock->GetDesc(&desc);
    bool result=false;
    bool ambiguous=false;
    bool descriptor_mismatch=false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto it=g_v356_backing_witnesses.find(
            reinterpret_cast<std::uintptr_t>(backing));
        if (it!=g_v356_backing_witnesses.end()) {
            const companion_set *match=nullptr;
            std::uint64_t unique_hash=0u;
            // Reject the ENTIRE backing when two simultaneously live exact
            // logical identities disagree, even if one view's desc matches.
            for (const auto &w:it->second) {
                const auto known=g_cache.find(w.view_key);
                if (known==g_cache.end() ||
                    g_ambiguous_view_device.count(w.view_key)!=0u ||
                    known->second.device!=device ||
                    known->second.logical_hash!=w.logical_hash) {
                    ambiguous=true;
                    break;
                }
                if (unique_hash!=0u && unique_hash!=w.logical_hash) {
                    ambiguous=true;
                    break;
                }
                unique_hash=w.logical_hash;
                if (v356_same_exact_view(desc,w.desc)) {
                    match=&known->second;
                } else {
                    descriptor_mismatch=true;
                }
            }
            if (!ambiguous && match!=nullptr &&
                unique_hash!=0u) {
                ID3D11ShaderResourceView *selected=nullptr;
                switch(cls) {
                    case asset_class::specular: selected=match->specular;break;
                    case asset_class::diffuse: selected=match->diffuse;break;
                    case asset_class::normal: selected=match->normal;break;
                }
                hash=unique_hash;
                if (selected!=nullptr && retain_view) {
                    selected->AddRef();
                    view=selected;
                }
                result=true;
            }
        }
    }
    device->Release();
    backing->Release();
    if (result) {
        const auto n=g_v356_exact_alias_hits.fetch_add(
            1u,std::memory_order_relaxed)+1u;
        if (n==1u)
            reshade::log::message(reshade::log::level::info,
                "[DSRRL V356 EXACT BACKING] stage=sidecar_recovered"
                " backing=IDENTICAL native_srv_desc=IDENTICAL"
                " donor_hash=FROM_LIVE_CERTIFIED_VIEW"
                " owner_receiver_gate=UNCHANGED pixel=OPEN");
    } else if (ambiguous) {
        g_v356_alias_ambiguous.fetch_add(1u,std::memory_order_relaxed);
    } else if (descriptor_mismatch) {
        g_v356_alias_desc_reject.fetch_add(1u,std::memory_order_relaxed);
    }
    return result;
}
#endif

'''
replace(needle,helper+needle,"alias lookup helper")

replace('''        cached.epoch = current_epoch;
        return publish_from_cache();
    }

    std::uint64_t logical_hash = 0u;''',
'''        cached.epoch = current_epoch;
        if (publish_from_cache()) return true;
#if defined(DSRRL_V356_EXACT_BACKING_SRV_ALIAS)
        return v356_snapshot_exact_backing(
            stock,cls,hash,view,retain_view);
#else
        return false;
#endif
    }

    std::uint64_t logical_hash = 0u;''',"missed TLS alias lookup")

replace('''    return publish_from_cache();
}

void logical_hashes_for(''',
'''    if (publish_from_cache()) return true;
#if defined(DSRRL_V356_EXACT_BACKING_SRV_ALIAS)
    return v356_snapshot_exact_backing(
        stock,cls,hash,view,retain_view);
#else
    return false;
#endif
}

void logical_hashes_for(''',"cold missed alias lookup")

replace('''    if (accepted)
        set = {};

    if (had_old)
        release_set(old);''',
'''    if (accepted)
        set = {};

#if defined(DSRRL_V356_EXACT_BACKING_SRV_ALIAS)
    // Register backing aliases only from a positively accepted exact named
    // stock view and only after independent native SRV->resource verification.
    // The V323 resource-name source join and source allowlist remain upstream.
    if (accepted) {
        auto *native_srv=reinterpret_cast<ID3D11ShaderResourceView *>(
            static_cast<std::uintptr_t>(view.handle));
        ID3D11Resource *backing=nullptr;
        native_srv->GetResource(&backing);
        if (backing!=nullptr) {
            const auto backing_key=reinterpret_cast<std::uintptr_t>(backing);
            if (backing_key==static_cast<std::uintptr_t>(
                    source_resource.handle)) {
                D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
                native_srv->GetDesc(&desc);
                try {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    v356_unbind_named_view(key);
                    auto &list=g_v356_backing_witnesses[backing_key];
                    list.push_back({key,logical_hash,native_device,desc});
                    g_v356_backing_by_view[key]=backing_key;
                } catch (...) {
                    // No speculative association if the provenance registry
                    // cannot be published atomically.
                    std::lock_guard<std::mutex> lock(g_mutex);
                    v356_unbind_named_view(key);
                }
            }
            backing->Release();
        }
    }
#endif

    if (had_old)
        release_set(old);''',"publish certified named view witness")

replace('''        const auto key =
            static_cast<std::uint64_t>(
                view.handle);

        if (g_ambiguous_view_device.erase(key) != 0u)''',
'''        const auto key =
            static_cast<std::uint64_t>(
                view.handle);
#if defined(DSRRL_V356_EXACT_BACKING_SRV_ALIAS)
        v356_unbind_named_view(key);
#endif

        if (g_ambiguous_view_device.erase(key) != 0u)''',"retire named view")

path.write_text(s,encoding="utf8")
print("V356_EXACT_SAME_RESOURCE_AND_VIEW_SIDECAR_RECOVERY_APPLIED")
