#!/usr/bin/env python3
"""V359b: original retail TLS GI name can be live at init_resource_view,
not at init_resource. Transport only across COM-exact native source backing.
This repairs producer/view timing while preserving independent map attestation.
Run after materialize_v359_attested_gi_name_alias.py.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/"src/runtime/envspec_resource_runtime.cpp"
s=p.read_text("utf8")
old="""void on_init_resource_view(
    device *device_ptr,
    resource resource_handle,
    resource_usage usage,
    const resource_view_desc &,
    resource_view view)
{
    if (device_ptr == nullptr ||
        resource_handle.handle == 0u ||
        view.handle == 0u ||
        (usage &
         resource_usage::shader_resource) ==
            resource_usage::undefined)
        return;

    std::lock_guard<std::mutex> lock(
        g_mutex);

    if (device_ptr != g_device)
        return;

    const auto native ="""
new=r'''void on_init_resource_view(
    device *device_ptr,
    resource resource_handle,
    resource_usage usage,
    const resource_view_desc &,
    resource_view view)
{
    if (device_ptr == nullptr ||
        resource_handle.handle == 0u ||
        view.handle == 0u ||
        (usage &
         resource_usage::shader_resource) ==
            resource_usage::undefined)
        return;

#if defined(DSRRL_V359_ATTESTED_GI_NAME_ALIAS)
    std::uint16_t gi_view_ordinal=0u;
    std::uint8_t gi_view_map=0u;
    const wchar_t *gi_view_name=nullptr;
    std::size_t gi_view_length=0u;
    bool gi_view_exact=
        device_ptr->get_api()==device_api::d3d11 &&
        texture_identity_transport::snapshot_raw(
            gi_view_name,gi_view_length) &&
        v359_match_original_gi_name(
            gi_view_name,gi_view_length,
            gi_view_ordinal,gi_view_map);
    if(gi_view_exact) {
        // ReShade source_resource is not by itself native COM provenance.
        // Demand a pointer-identical D3D11 SRV backing and device identity.
        auto *srv=reinterpret_cast<ID3D11ShaderResourceView *>(
            static_cast<std::uintptr_t>(view.handle));
        ID3D11Resource *backing=nullptr;
        ID3D11Device *device=nullptr;
        srv->GetResource(&backing);
        srv->GetDevice(&device);
        gi_view_exact=backing!=nullptr && device!=nullptr &&
            reinterpret_cast<std::uintptr_t>(backing)==
                static_cast<std::uintptr_t>(resource_handle.handle) &&
            reinterpret_cast<std::uintptr_t>(device)==
                static_cast<std::uintptr_t>(device_ptr->get_native());
        if(backing!=nullptr)backing->Release();
        if(device!=nullptr)device->Release();
    }
#endif

    std::lock_guard<std::mutex> lock(
        g_mutex);

    if (device_ptr != g_device)
        return;

#if defined(DSRRL_V359_ATTESTED_GI_NAME_ALIAS)
    if(gi_view_exact) {
        const auto witness=g_v358_lifetime_probes.find(
            resource_handle.handle);
        if(witness!=g_v358_lifetime_probes.end()) {
            auto &w=witness->second;
            if(w.source_name_exact &&
               (w.source_ordinal!=gi_view_ordinal ||
                w.source_map!=gi_view_map)) {
                // Two original retail names claim the same live resource:
                // permanently poison the alias for both map families.
                g_v359_map_conflict[w.source_map]=true;
                g_v359_map_conflict[gi_view_map]=true;
                w.source_name_exact=false;
            } else if(w.exact_data) {
                w.source_name_exact=true;
                w.source_ordinal=gi_view_ordinal;
                w.source_map=gi_view_map;
                if(w.certified) {
                    if(w.probe!=gi_view_ordinal) {
                        g_v359_map_conflict[gi_view_map]=true;
                    } else {
                        auto &mask=g_v359_seen_ordinal[gi_view_map]
                           [gi_view_ordinal/64u];
                        const auto bit=1ull<<(gi_view_ordinal%64u);
                        if((mask&bit)==0u) {
                            mask|=bit;
                            ++g_v359_seen_count[gi_view_map];
                        }
                    }
                }
            }
        }
    }
#endif

    const auto native ='''
if s.count(old)!=1:raise RuntimeError("V359b exact view callback drift")
p.write_text(s.replace(old,new),"utf8")
print("V359_VIEW_SCOPE_EXACT_SOURCE_IDENTITY_RECOVERY_PASS")
