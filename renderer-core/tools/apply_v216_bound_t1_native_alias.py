#!/usr/bin/env python3
"""V216: exact live native D3D11 stock-t1 resource->alternate SRV join.

Use only when existing FLVER+slot+MTD exact EnvSpec owner is already verified
at an RX33-35 draw. The independently verified PTDE shader consumer remains
mandatory; this script never arms or simulates the consumer.
"""
from pathlib import Path
import argparse
CPP=Path(__file__).resolve().parents[2]/"renderer-core/src/runtime/material_resource_draw_runtime.cpp"

def once(s, old, new):
    if s.count(new)==1:return s
    if s.count(old)!=1:raise RuntimeError("V216 source mismatch "+repr(old[:100])+" count "+str(s.count(old)))
    return s.replace(old,new,1)

def patch(s):
    s=once(s,
        '#include "dsrrl/runtime/v214_resource_name_provenance.hpp"',
        '#include "dsrrl/runtime/v214_resource_name_provenance.hpp"\n'
        '#include "dsrrl/runtime/v216_native_resource_alias.hpp"')

    s=once(s,
        '    if (accepted)\n        set = {};\n\n    if (had_old)\n        release_set(old);',
        '''    if (accepted) {
#if defined(DSRRL_V203_FULL_SIDECAR_1295)
        if (spec_member && sidecar_20261010::spec_allowed(logical_hash) &&
            source_resource.handle != 0u) {
            auto *srv=reinterpret_cast<ID3D11ShaderResourceView *>(view.handle);
            ID3D11Resource *backing=nullptr;
            srv->GetResource(&backing);
            const bool exact_backing=backing!=nullptr &&
                reinterpret_cast<std::uintptr_t>(backing)==
                static_cast<std::uintptr_t>(source_resource.handle);
            if (backing)backing->Release();
            if (exact_backing)
                (void)v216_native_resource_alias::instance().observe(
                    static_cast<std::uintptr_t>(device->get_native()),
                    static_cast<std::uintptr_t>(source_resource.handle),
                    static_cast<std::uintptr_t>(view.handle),logical_hash);
        }
#endif
        set = {};
    }

    if (had_old)
        release_set(old);''')

    s=once(s,
        '''void on_destroy_resource_view(
    reshade::api::device *,
    reshade::api::resource_view view)
{
    companion_set dead{};''',
        '''void on_destroy_resource_view(
    reshade::api::device *,
    reshade::api::resource_view view)
{
    v216_native_resource_alias::instance().destroy_view(
        static_cast<std::uintptr_t>(view.handle));
    companion_set dead{};''')

    s=once(s,
        '''    v214_resource_provenance::instance().destroy(
        static_cast<std::uintptr_t>(device->get_native()),
        static_cast<std::uintptr_t>(resource.handle));''',
        '''    v214_resource_provenance::instance().destroy(
        static_cast<std::uintptr_t>(device->get_native()),
        static_cast<std::uintptr_t>(resource.handle));
    v216_native_resource_alias::instance().destroy_resource(
        static_cast<std::uintptr_t>(device->get_native()),
        static_cast<std::uintptr_t>(resource.handle));''')

    s=once(s,
        '''    v214_resource_provenance::instance().destroy_device(
        reinterpret_cast<std::uintptr_t>(native));
    release_cache_for_device(
        native);''',
        '''    v214_resource_provenance::instance().destroy_device(
        reinterpret_cast<std::uintptr_t>(native));
    v216_native_resource_alias::instance().destroy_device(
        reinterpret_cast<std::uintptr_t>(native));
    release_cache_for_device(
        native);''')

    func='''#if defined(DSRRL_V203_FULL_SIDECAR_1295)
bool try_v216_bound_t1_alias(
    ID3D11ShaderResourceView *stock_t1,
    std::uint64_t &logical_hash,
    ID3D11ShaderResourceView *&companion) noexcept
{
    if (!stock_t1 || logical_hash || companion)return false;
    ID3D11Resource *resource=nullptr;
    stock_t1->GetResource(&resource);
    if (!resource)return false;
    ID3D11Device *device=nullptr;
    stock_t1->GetDevice(&device);
    if (!device) {resource->Release();return false;}

    v216_native_resource_alias::anchor witness{};
    const bool exact=v216_native_resource_alias::instance().resolve(
        reinterpret_cast<std::uintptr_t>(device),
        reinterpret_cast<std::uintptr_t>(resource),
        witness);
    device->Release();
    resource->Release();
    if (!exact || !witness.view ||
        !sidecar_20261010::spec_allowed(witness.logical_hash))
        return false;

    // The original named owner view must remain live in the exact
    // per-view companion registry; otherwise fail open.
    {
        std::lock_guard<std::mutex> g(g_mutex);
        const auto found=g_cache.find(
            static_cast<std::uint64_t>(witness.view));
        if (found==g_cache.end() ||
            g_ambiguous_view_device.find(
                static_cast<std::uint64_t>(witness.view))!=
                g_ambiguous_view_device.end() ||
            found->second.logical_hash!=witness.logical_hash ||
            !found->second.specular)
            return false;
        found->second.specular->AddRef();
        companion=found->second.specular;
        logical_hash=witness.logical_hash;
    }
    static std::atomic<std::uint32_t> positives{0u};
    const auto n=positives.fetch_add(1u,std::memory_order_relaxed)+1u;
    if (n<=12u || (n<=8192u && (n&(n-1u))==0u)) {
        char line[320]{};
        std::snprintf(line,sizeof(line),
            "[DSRRL V216 BOUND T1] stage=exact_resource_view_alias "
            "name=%016llx companion=1 consumer_permission=UNCHANGED "
            "swap=0 pixel=OPEN",
            static_cast<unsigned long long>(logical_hash));
        reshade::log::message(reshade::log::level::info,line);
    }
    return true;
}
#endif

'''
    s=once(s,'''bool material_resource_draw_runtime::
prepare_draw_requests_bound(''',func+'''bool material_resource_draw_runtime::
prepare_draw_requests_bound(''')

    s=once(s,
        '''    const auto h0 = hashes[0];
    const auto h1 = hashes[1];
    const auto h2 = hashes[2];''',
        '''    const auto h0 = hashes[0];
    auto h1 = hashes[1];
    const auto h2 = hashes[2];
#if defined(DSRRL_V203_FULL_SIDECAR_1295)
    if (h1==0u && companions[1]==nullptr &&
        receiver_id>=33u && receiver_id<=35u &&
        sidecar_20261010::exact_nonmetal_material(query.material))
        (void)try_v216_bound_t1_alias(
            views[1],h1,companions[1]);
#endif''')
    return s

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--apply",action="store_true")
    args=p.parse_args()
    s=CPP.read_text(encoding="utf-8").replace("\r\n","\n")
    patched=patch(s)
    if args.apply and patched!=s:
        CPP.write_text(patched,encoding="utf-8",newline="\n")
    elif not args.apply and patched!=s:
        raise RuntimeError("V216 overlay absent")
    print("V216_BOUND_T1_RESOURCE_ALIAS_"+("APPLIED" if args.apply else "VERIFIED"))
    print("V216_SCOPE=EXACT_NONMETAL_RX33_35")
    print("V216_CONSUMER_GATE_UNCHANGED=TRUE")
