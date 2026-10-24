#!/usr/bin/env python3
"""V214 exact original CPU logical name -> D3D11 resource -> SRV adapter.

Runs AFTER V203/V204/V206 generation. No wholesale transport rollback.
The retail TLS name must overlap ReShade's exact init_resource callback and
the generated view must GetResource() to the same native ID3D11Resource.
If the producer did not overlap, leave unnamed stock SRV untouched (fail-open).
Full 1295 PTDE Specular names are admitted by the asset loader, but final
operator authorization still demands exact owner/consumer and native routing.
"""
from pathlib import Path
import argparse

ROOT=Path(__file__).resolve().parents[2]
CPP=ROOT/"renderer-core/src/runtime/material_resource_draw_runtime.cpp"

def once(s: str, old: str, new: str) -> str:
    if s.count(new)==1:
        return s
    if s.count(old)!=1:
        raise RuntimeError(f"V214 source mismatch for anchor {old[:75]!r}: {s.count(old)}")
    return s.replace(old,new,1)

def patch_v230_view_witness(s: str) -> str:
    """Expose exact view-init origin for missed RX34 sidecars; no routing grants."""
    if "enum class v230_view_stage" not in s:
        return s
    begin=s.index("void witness_rx34_resource_identity(")
    end=s.index("void on_init_resource_view(",begin)
    witness=s[begin:end]
    witness=once(witness,
        "        std::uint64_t registry_hash = 0u;",
        "        std::uint64_t registry_hash = 0u;\n"
        "#if defined(DSRRL_V230_RUNTIME_TELEMETRY)\n"
        "        v230_view_origin origin{};\n"
        "        bool origin_found = false;\n"
        "#endif")
    witness=once(witness,
        "            const auto found = g_cache.find(key);",
        "#if defined(DSRRL_V230_RUNTIME_TELEMETRY)\n"
        "            const auto observed = g_v230_view_origin.find(key);\n"
        "            if (observed != g_v230_view_origin.end()) {\n"
        "                origin=observed->second; origin_found=true;\n"
        "            }\n"
        "#endif\n"
        "            const auto found = g_cache.find(key);")
    # Deduplicate repeated draw calls by exact t0/t1/t2 view handles and
    # requested slots, otherwise six initial draws hide later equipment.
    old_sample="""    auto sample = g_rx34_identity_witness_samples.load(
        std::memory_order_relaxed);
    if (sample >= 6u ||
        !g_rx34_identity_witness_samples.compare_exchange_strong(
            sample, sample + 1u, std::memory_order_relaxed))
        return;
"""
    new_sample="""#if defined(DSRRL_V230_RUNTIME_TELEMETRY)
    static std::mutex witness_guard;
    static std::array<std::array<std::uint64_t, 4>, 96> seen{};
    static std::size_t seen_count = 0;
    const std::array<std::uint64_t,4> signature{
        static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(views[0])),
        static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(views[1])),
        static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(views[2])),
        static_cast<std::uint64_t>(requested_mask)
    };
    std::uint32_t sample = 0u;
    {
        std::lock_guard<std::mutex> lock(witness_guard);
        if (std::find(seen.begin(),seen.begin()+seen_count,signature)
                != seen.begin()+seen_count || seen_count==seen.size())
            return;
        sample = static_cast<std::uint32_t>(seen_count++);
        seen[sample] = signature;
    }
#else
""" + old_sample + """#endif
"""
    witness=once(witness,old_sample,new_sample)
    # Single exact stock SRV record. This is observation, not authority.
    witness=once(witness,
        "sidecar=%u reason=%s\",",
        "sidecar=%u reason=%s\"\n"
        "#if defined(DSRRL_V230_RUNTIME_TELEMETRY)\n"
        "            \" view_init=%s backing=%016llx name=%016llx eligible=%u uploaded=%u\"\n"
        "#endif\n            ,")
    witness=once(witness,
        "            reason);",
        "            reason\n"
        "#if defined(DSRRL_V230_RUNTIME_TELEMETRY)\n"
        "            , origin_found ? v230_stage_name(origin.stage) :\n"
        "                \"NO_VIEW_INIT_OBSERVATION\",\n"
        "            static_cast<unsigned long long>(origin.resource_handle),\n"
        "            static_cast<unsigned long long>(origin.logical_hash),\n"
        "            static_cast<unsigned>(origin.class_mask),\n"
        "            static_cast<unsigned>(origin.uploaded_mask)\n"
        "#endif\n            );")
    s=s[:begin]+witness+s[end:]
    return s


def patch(source: str) -> str:
    s=source
    s=once(s,
        '#include "dsrrl/runtime/texture_identity_transport.hpp"',
        '#include "dsrrl/runtime/texture_identity_transport.hpp"\n'
        '#include "dsrrl/runtime/v214_resource_name_provenance.hpp"')

    # V230 diagnostic source names this argument resource_handle. Adapt
    # its ABI here, before the original V323 source_resource binding.
    s=s.replace('    reshade::api::resource resource_handle,\n    reshade::api::resource_usage usage,', '    reshade::api::resource,\n    reshade::api::resource_usage usage,')

    # V322 already uses spec_equipment_allowed(): exact Oct10 749 direct names
    # plus the original owner and consumer gates; do not widen its allowlist.
    s=once(s,
        '    reshade::api::resource,\n    reshade::api::resource_usage usage,',
        '    reshade::api::resource source_resource,\n    reshade::api::resource_usage usage,')

    # Keep diagnostic handles bound to the V323-certified source argument.
    s=s.replace('v230_observe_view(view.handle, resource_handle.handle,',
                'v230_observe_view(view.handle, source_resource.handle,')
    s=s.replace('v230_observe_view(view.handle,resource_handle.handle,',
                'v230_observe_view(view.handle,source_resource.handle,')

    anchor='void on_init_resource_view(\n'
    producer="""// Exact native ID3D11Resource is the transport key. This does not infer the
// GPU identity from a similar texture name or MTD. The ReShade callback may
// re-fire for the same resource: conflicting names permanently poison it.
void on_init_resource(
    reshade::api::device *device,
    const reshade::api::resource_desc &desc,
    const reshade::api::subresource_data *,
    reshade::api::resource_usage,
    reshade::api::resource resource)
{
    if (g_internal_create || g_quarantined.load() ||
        device == nullptr ||
        device->get_api() != reshade::api::device_api::d3d11 ||
        resource.handle == 0u ||
        desc.type != reshade::api::resource_type::texture_2d ||
        (static_cast<std::uint32_t>(desc.usage) &
         static_cast<std::uint32_t>(
             reshade::api::resource_usage::shader_resource)) == 0u)
        return;

    const wchar_t *name=nullptr;
    std::size_t n=0u;
    if (!texture_identity_transport::snapshot_raw(name,n) ||
        n==0u || n>256u)
        return;
    const auto hash=fnv_name(name,n);
    if (!spec_equipment_allowed(hash) &&
        !generated::spec_equipment_name_hash_allowed_v12(hash) &&
        !generated::diffuse_name_hash_allowed_v12(hash) &&
        !generated::normal_name_hash_allowed_v12(hash))
        return;
    (void)v214_resource_provenance::instance().observe(
        static_cast<std::uintptr_t>(device->get_native()),
        static_cast<std::uintptr_t>(resource.handle),name,n);
}

void on_destroy_resource(
    reshade::api::device *device,
    reshade::api::resource resource)
{
    if (device == nullptr ||
        device->get_api() != reshade::api::device_api::d3d11)
        return;
    v214_resource_provenance::instance().destroy(
        static_cast<std::uintptr_t>(device->get_native()),
        static_cast<std::uintptr_t>(resource.handle));
}

"""
    s=once(s,anchor,producer+anchor)

    old="""    const wchar_t *logical_name_raw = nullptr;
    std::size_t logical_name_length = 0u;
    if (!texture_identity_transport::snapshot_raw(
            logical_name_raw,
            logical_name_length))
        return;

    const auto logical_hash =
"""
    new="""    const wchar_t *logical_name_raw = nullptr;
    std::size_t logical_name_length = 0u;
    const bool direct_tls_name =
        texture_identity_transport::snapshot_raw(
            logical_name_raw,logical_name_length);
    std::array<wchar_t,257> source_name{};
    std::size_t source_len=0u;
    const auto source_status =
        v214_resource_provenance::instance().snapshot(
            static_cast<std::uintptr_t>(device->get_native()),
            static_cast<std::uintptr_t>(source_resource.handle),
            source_name,source_len);

    // An independently observed contradictory source poisons this binding,
    // even when a direct TLS name exists.
    if (source_status==v214_resource_provenance::status::ambiguous)
        return;
    if (direct_tls_name) {
        if (source_status==v214_resource_provenance::status::exact &&
            (logical_name_length!=source_len ||
             !std::equal(source_name.begin(),
                         source_name.begin()+source_len,
                         logical_name_raw)))
            return;
        // V215: the owner runtime proves logical Specular names are present
        // at init_resource_view (16 confirmed ready loads), while V214's
        // init_resource-only seed may miss the retail producer lifetime.
        // A direct, exact native SRV->ID3D11Resource witness can seed the
        // SAME resource for later distinct SRVs, without FLVER inference.
        // Names not present in the certified sidecar corpus never seed.
        if (source_status==v214_resource_provenance::status::absent &&
            logical_name_length>=3u && logical_name_length<=256u) {
            const auto direct_hash=fnv_name(
                logical_name_raw,logical_name_length);
            if (spec_equipment_allowed(direct_hash) ||
                generated::spec_equipment_name_hash_allowed_v12(
                    direct_hash) ||
                generated::diffuse_name_hash_allowed_v12(direct_hash) ||
                generated::normal_name_hash_allowed_v12(direct_hash)) {
                auto *srv=reinterpret_cast<ID3D11ShaderResourceView *>(
                    view.handle);
                ID3D11Resource *native_backing=nullptr;
                srv->GetResource(&native_backing);
                const bool resource_exact=native_backing!=nullptr &&
                    reinterpret_cast<std::uintptr_t>(native_backing)==
                    static_cast<std::uintptr_t>(source_resource.handle);
                if (native_backing)native_backing->Release();
                if (!resource_exact)
                    return;
                if (!v214_resource_provenance::instance().observe(
                        static_cast<std::uintptr_t>(device->get_native()),
                        static_cast<std::uintptr_t>(
                            source_resource.handle),
                        logical_name_raw,logical_name_length))
                    return;
                static std::atomic<std::uint32_t> published{0u};
                const auto n=published.fetch_add(
                    1u,std::memory_order_relaxed)+1u;
                if (false && (n<=8u || (n<=8192u && (n&(n-1u))==0u)))
                    reshade::log::message(reshade::log::level::info,
                        "[DSRRL V215 VIEW SOURCE] stage=exact_view_backseed "
                        "source=RETAIL_TLS_SRV resource_backing=EXACT "
                        "name=WHITELISTED consumer_permission=UNCHANGED "
                        "swap=0 pixel=OPEN");
            }
        }
    } else {
        if (source_status!=v214_resource_provenance::status::exact ||
            source_len==0u)
            return;
        // Verify ReShade's resource handle against the real COM backing SRV.
        // This rejects stale, reconstructed and misrouted CPU resource keys.
        auto *srv=reinterpret_cast<ID3D11ShaderResourceView *>(view.handle);
        ID3D11Resource *native_backing=nullptr;
        srv->GetResource(&native_backing);
        const bool backing_exact=native_backing!=nullptr &&
            reinterpret_cast<std::uintptr_t>(native_backing)==
            static_cast<std::uintptr_t>(source_resource.handle);
        if (native_backing)
            native_backing->Release();
        if (!backing_exact)
            return;
        logical_name_raw=source_name.data();
        logical_name_length=source_len;
        static std::atomic<std::uint32_t> source_join_count{0u};
        const auto n=source_join_count.fetch_add(
            1u,std::memory_order_relaxed)+1u;
        if (false && (n<=8u || (n<=4096u && (n&(n-1u))==0u)))
            reshade::log::message(reshade::log::level::info,
                "[DSRRL V214 RESOURCE NAME] stage=exact_native_resource_to_srv "
                "owner=CPU_TEX_NAME resource_backing=EXACT "
                "consumer_permission=UNCHANGED swap=0 pixel=OPEN");
    }

    const auto logical_hash =
"""
    # V230 view-origin evidence must not preempt the exact V214 resource
    # fallback. Normalize the original callback stanza before V323 routing;
    # the no-name evidence is added only AFTER both certified sources fail.
    diagnostic_old=old.replace(
        '            logical_name_length))\n        return;',
        '            logical_name_length)) {\n'
        '#if defined(DSRRL_V230_RUNTIME_TELEMETRY)\n'
        '        v230_observe_view(view.handle, source_resource.handle, 0u,\n'
        '            v230_view_stage::no_name_scope, 0u, 0u);\n'
        '#endif\n        return;\n    }')
    if diagnostic_old in s:
        s=once(s,diagnostic_old,old)
        # Only after V214's native resource/SRV equality path failed is the
        # stock view truly without a certified name. This is diagnostic only.
        new=new.replace(
            '            source_len==0u)\n            return;',
            '            source_len==0u) {\n'
            '#if defined(DSRRL_V230_RUNTIME_TELEMETRY)\n'
            '            v230_observe_view(view.handle,source_resource.handle,0u,\n'
            '                v230_view_stage::no_name_scope,0u,0u);\n'
            '#endif\n            return;\n        }')
    s=once(s,old,new)

    s=once(s,
        '    release_cache_for_device(\n        native);\n}',
        '    v214_resource_provenance::instance().destroy_device(\n'
        '        reinterpret_cast<std::uintptr_t>(native));\n'
        '    release_cache_for_device(\n        native);\n}')

    s=once(s,
        '    reshade::register_event<\n'
        '        reshade::addon_event::init_resource_view>(\n'
        '            on_init_resource_view);',
        '    reshade::register_event<\n'
        '        reshade::addon_event::init_resource>(on_init_resource);\n'
        '    reshade::register_event<\n'
        '        reshade::addon_event::destroy_resource>(on_destroy_resource);\n'
        '    reshade::register_event<\n'
        '        reshade::addon_event::init_resource_view>(\n'
        '            on_init_resource_view);')

    s=once(s,
        '    reshade::unregister_event<\n'
        '        reshade::addon_event::init_resource_view>(\n'
        '            on_init_resource_view);\n\n    release_cache();',
        '    reshade::unregister_event<\n'
        '        reshade::addon_event::init_resource_view>(\n'
        '            on_init_resource_view);\n'
        '    reshade::unregister_event<\n'
        '        reshade::addon_event::destroy_resource>(on_destroy_resource);\n'
        '    reshade::unregister_event<\n'
        '        reshade::addon_event::init_resource>(on_init_resource);\n\n'
        '    release_cache();')

    return patch_v230_view_witness(s)

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--apply",action="store_true")
    args=parser.parse_args()
    original=CPP.read_text(encoding="utf-8").replace("\r\n","\n")
    updated=patch(original)
    if args.apply and updated!=original:
        CPP.write_text(updated,encoding="utf-8",newline="\n")
    elif not args.apply and updated!=original:
        raise RuntimeError("V214 overlay not applied: "+str(CPP))
    print("V323_EXACT_RESOURCE_NAME_SRV_"+("APPLIED" if args.apply else "VERIFIED"))
    print("V214_FULL_SPEC_1295_ASSET_LOADER=ON")
    print("V214_FLVER_MTD_CONSUMER_GATE_UNCHANGED=ON")
    print("V214_NO_SOURCE_PROVENANCE=FAIL_OPEN")

# V230 diagnostic compatibility: exact original-resource provenance is mandatory.
