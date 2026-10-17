#!/usr/bin/env python3
"""V356 narrow EnvSpec recovery for alternative native cube SRVs of the SAME
already SHA256-certified BC6H resource. Unknown hashes still fail-open.
Must run after V354 resource patch; no new probe mapping, no guessed ordinal.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/"src/runtime/envspec_resource_runtime.cpp"
s=p.read_text("utf8")
def once(a,b,label):
 global s
 n=s.count(a)
 if n!=1:raise RuntimeError(f"V356 env alias anchor {label}: {n}")
 s=s.replace(a,b,1)

anchor='''bool probe_for_native_view(
    ID3D11ShaderResourceView *view,
    std::uint16_t &probe) noexcept
{'''
impl=r'''#if defined(DSRRL_V356_EXACT_BACKING_SRV_ALIAS)
std::atomic<std::uint64_t> g_v356_probe_same_backing_rescued{0u};

// Caller owns g_mutex. Unmapped view alone is NOT proof of a native probe.
// A genuinely SHA-verified retail resource created in on_init_resource()
// may have several equivalent native cube SRVs; only that exact same COM
// backing and a complete matching cube SRV authorize the existing ordinal.
bool v356_resolve_exact_native_backing_locked(
    ID3D11ShaderResourceView *view,
    std::uint16_t &probe) noexcept
{
    if (view==nullptr || g_device==nullptr) return false;
    const auto view_key=static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(view));
    const auto named=g_resource_by_view.find(view_key);
    if (named!=g_resource_by_view.end()) {
        probe=named->second.probe_ordinal;
        return true;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
    view->GetDesc(&desc);
    if (desc.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURECUBE ||
        desc.TextureCube.MostDetailedMip!=0u ||
        desc.TextureCube.MipLevels!=k_dsr_levels ||
        (desc.Format!=DXGI_FORMAT_BC6H_UF16 &&
         desc.Format!=DXGI_FORMAT_BC6H_SF16))
        return false;

    ID3D11Resource *backing=nullptr;
    view->GetResource(&backing);
    if (backing==nullptr) return false;
    ID3D11Device *native_device=nullptr;
    view->GetDevice(&native_device);
    const bool same_device =
        native_device!=nullptr &&
        reinterpret_cast<std::uintptr_t>(native_device)==static_cast<std::uintptr_t>(g_device->get_native());
    const auto it=g_native_resources.find(
        static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(backing)));
    const bool ready=same_device && it!=g_native_resources.end();
    if (ready) probe=it->second.probe_ordinal;
    if (native_device!=nullptr) native_device->Release();
    backing->Release();
    if (ready) {
        const auto n=g_v356_probe_same_backing_rescued.fetch_add(
            1u,std::memory_order_relaxed)+1u;
        if (n==1u)
            reshade::log::message(reshade::log::level::info,
                "[DSRRL V356 PROBE BACKING]"
                " stage=exact_SHA_verified_resource_rescued"
                " view=COMPLETE_NATIVE_BC6H_CUBE"
                " unknown_probe_alias=FORBIDDEN pixel=OPEN");
    }
    return ready;
}
#endif

'''
once(anchor,impl+anchor,"helper")
before='''    const auto by_view =
        g_resource_by_view.find(key);
    if (by_view ==
        g_resource_by_view.end())
        return false;

    probe =
        by_view->second.probe_ordinal;
    return true;
}

bool snapshot_ready_envspec('''
after='''#if defined(DSRRL_V356_EXACT_BACKING_SRV_ALIAS)
    (void)key;
    return v356_resolve_exact_native_backing_locked(view,probe);
#else
    const auto by_view =
        g_resource_by_view.find(key);
    if (by_view ==
        g_resource_by_view.end())
        return false;

    probe =
        by_view->second.probe_ordinal;
    return true;
#endif
}

bool snapshot_ready_envspec('''
once(before,after,"probe_for_native_view")

before='''            const auto by_view =
                g_resource_by_view.find(view_key);
            if (by_view ==
                g_resource_by_view.end())
                return false;

            probe =
                by_view->second.probe_ordinal;
            return true;
        };

    std::lock_guard<std::mutex> lock(g_mutex);'''
after='''#if defined(DSRRL_V356_EXACT_BACKING_SRV_ALIAS)
            (void)view_key;
            return v356_resolve_exact_native_backing_locked(view,probe);
#else
            const auto by_view =
                g_resource_by_view.find(view_key);
            if (by_view ==
                g_resource_by_view.end())
                return false;

            probe =
                by_view->second.probe_ordinal;
            return true;
#endif
        };

    std::lock_guard<std::mutex> lock(g_mutex);'''
once(before,after,"snapshot resolver")

p.write_text(s,encoding="utf8")
print("V356_ENVSPEC_NATIVE_BC6H_SHA_RESOURCE_SRV_ALIAS_PATCH_PASS")
