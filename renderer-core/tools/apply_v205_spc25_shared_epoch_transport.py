#!/usr/bin/env python3
"""V205: port authenticated SPC25 808D CPU-writer -> physical PS t1 identity.

Source-complete: exact historical SPC25 input snapshots and epoch transform
are committed in renderer-core/data/spc25_v205. This transformer operates ONLY
on temporary post-R43/PR306 generated sources. Protected P_Metal/PointLight,
GPU state, DSR executable and sidecar bytes are never changed.

All receivers RX24..35 may be OBSERVED. There is NO cross-family SpecRGB
binding authorization: a CPU name/resource/epoch match is a diagnostic
source/transport witness, not evidence of an equipment or PTDE operator.
"""
from __future__ import annotations
import argparse
import importlib.util
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
OLD=ROOT/"renderer-core/data/spc25_v205"
TEX=ROOT/"renderer-core/src/runtime/texture_identity_transport.cpp"
MAT=ROOT/"renderer-core/src/runtime/material_resource_draw_runtime.cpp"
FLAG="DSRRL_V205_SPC25_EPOCH_TRANSPORT"

def replace_once(s: str, old: str, new: str) -> str:
    if s.count(new)==1:
        return s
    if s.count(old)!=1:
        raise AssertionError("V205 exact generated-source anchor drift: "+
                             repr(old[:105])+" occurrences="+str(s.count(old)))
    return s.replace(old,new,1)

def legacy():
    import sys
    path=OLD/"apply_spc25_native_t1_epoch.py"
    spec=importlib.util.spec_from_file_location("spc25_v205_legacy_transform",path)
    assert spec and spec.loader
    mod=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

def transport(apply: bool) -> None:
    historical=(OLD/"texture_identity_transport.cpp").read_text(encoding="utf-8").replace("\r\n","\n")
    module=legacy()
    current=TEX.read_text(encoding="utf-8").replace("\r\n","\n")
    if apply:
        # We are replacing only the generated *temporary* runtime source.
        # The historical consumer source stays preserved in the Git tree.
        if FLAG in current and "SPC25_EXACT_SOURCE" in current:
            # Idempotent replay: already transformed; verify instead.
            module.process(TEX,module.TEXTURE,False)
            return
        if "dsrrl_texture_name_observer(" not in current:
            raise AssertionError("Pinned DSR retail texture hook missing")
        TEX.write_text(historical,encoding="utf-8",newline="\n")
        module.process(TEX,module.TEXTURE,True)
        output=TEX.read_text(encoding="utf-8")
        TEX.write_text(output+"\n// SPC25_EXACT_SOURCE "+FLAG+"\n",
                       encoding="utf-8",newline="\n")
    else:
        if FLAG not in current or "SPC25_EXACT_SOURCE" not in current:
            raise AssertionError("V205 pinned SPC25 transport not installed")
        # Strip only marker before using historical reverse-composition test.
        before=current.replace("\n// SPC25_EXACT_SOURCE "+FLAG+"\n","")
        TEX.write_text(before,encoding="utf-8",newline="\n")
        try:
            module.process(TEX,module.TEXTURE,False)
        finally:
            TEX.write_text(current,encoding="utf-8",newline="\n")

def material(s: str) -> str:
    s=replace_once(
        s,
        '#include "dsrrl/runtime/texture_identity_transport.hpp"',
        '#include "dsrrl/runtime/texture_identity_transport.hpp"\n'
        '#if defined(DSRRL_V205_SPC25_EPOCH_TRANSPORT)\n'
        '#include "dsrrl/runtime/native_ps_t1_view_lifetime.hpp"\n'
        '#endif')

    s=replace_once(s,
       "void on_init_resource_view(\n"
       "    reshade::api::device *device,\n"
       "    reshade::api::resource,\n",
       "void on_init_resource_view(\n"
       "    reshade::api::device *device,\n"
       "    reshade::api::resource resource,\n")
    anchor="    stutter_profile::scope view_time(stutter_profile::stage::resource_view_init);\n"
    addition="""#if defined(DSRRL_V205_SPC25_EPOCH_TRANSPORT)
    // Observe ALL native D3D11 shader views even if the loader's thread-local
    // resource name is missing; exact parent resource+generation are required.
    native_ps_t1_lifetime::instance().init(
        reinterpret_cast<std::uintptr_t>(device->get_native()),
        static_cast<std::uintptr_t>(view.handle),
        static_cast<std::uintptr_t>(resource.handle));
#endif
"""
    s=replace_once(s,anchor,addition+anchor)

    s=replace_once(s,
        "void on_destroy_resource_view(\n"
        "    reshade::api::device *,\n"
        "    reshade::api::resource_view view)\n"
        "{\n    companion_set dead{};",
        """void on_destroy_resource_view(
    reshade::api::device *device,
    reshade::api::resource_view view)
{
#if defined(DSRRL_V205_SPC25_EPOCH_TRANSPORT)
    if (device && device->get_api()==reshade::api::device_api::d3d11)
        native_ps_t1_lifetime::instance().destroy(
            reinterpret_cast<std::uintptr_t>(device->get_native()),
            static_cast<std::uintptr_t>(view.handle));
    texture_identity_transport::retire_native_ps_t1(
        reinterpret_cast<const void *>(
            static_cast<std::uintptr_t>(view.handle)));
#endif
    companion_set dead{};""")

    s=replace_once(s,
        "    release_cache_for_device(\n        native);\n}\n\n#if defined(DSRRL_STUTTER_PROFILE)",
        """#if defined(DSRRL_V205_SPC25_EPOCH_TRANSPORT)
    native_ps_t1_lifetime::instance().destroy_device(
        reinterpret_cast<std::uintptr_t>(native));
#endif
    release_cache_for_device(
        native);
}

#if defined(DSRRL_STUTTER_PROFILE)""")

    anchor="    const auto h2 = hashes[2];\n"
    diag="""#if defined(DSRRL_V205_SPC25_EPOCH_TRANSPORT)
    // Source/transport census shared by all observed DifSpcBmp receivers,
    // but without broadening ANY sidecar or material-consumer authority.
    // P_Metal route345 remains on its protected independent island.
    if (receiver_id>=24u && receiver_id<=35u &&
        query.material.owner_tuple_exact &&
        query.material.route_index!=345u &&
        views[1]!=nullptr && h1==0u &&
        texture_identity_transport::should_sample_native_ps_t1(views[1])) {
        ID3D11Resource *stock_resource=nullptr;
        views[1]->GetResource(&stock_resource);
        if (stock_resource) {
            const auto live_epoch =
                native_ps_t1_lifetime::instance().current(
                    reinterpret_cast<std::uintptr_t>(views[1]),
                    reinterpret_cast<std::uintptr_t>(stock_resource));
            std::array<char,65u> name{};
            const bool exact_writer =
                texture_identity_transport::
                    resolve_exact_equipment_native_ps_t1(
                        views[1],stock_resource,
                        name.data(),name.size());
            // Extra allowlist check is useful as a census, but does NOT
            // convert a named NPC/world resource into equipment authority.
            std::array<wchar_t,65u> unicode{};
            std::size_t len=0u;
            if (exact_writer) {
                for (;len<64u && name[len];++len)
                    unicode[len]=static_cast<unsigned char>(name[len]);
            }
            const auto name_hash=
                exact_writer?fnv_name(unicode.data(),len):0ull;
            const bool listed=exact_writer &&
                sidecar_20261010::spec_allowed(name_hash);
            texture_identity_transport::diagnose_native_ps_t1(
                views[1],stock_resource);
            static std::atomic<std::uint32_t> seen{0u};
            const auto count=seen.fetch_add(
                1u,std::memory_order_relaxed)+1u;
            if (count<=24u || (count<=8192u &&
                    (count & (count-1u))==0u) || exact_writer) {
                char line[490]{};
                std::snprintf(line,sizeof(line),
                    "[DSRRL V205 SPC25 SHARED T1] stage=native_cpu_srv_epoch "
                    "rx=%u route=%u slot=%u epoch=%llu "
                    "cpu_view_resource_exact=%u logical_name=%s "
                    "spec_sidecar_name_allowed=%u gspec_authority=0 "
                    "srv_swap=0 pixel=OPEN",
                    static_cast<unsigned>(receiver_id),
                    static_cast<unsigned>(query.material.route_index),
                    static_cast<unsigned>(query.material.material_slot),
                    static_cast<unsigned long long>(live_epoch.epoch),
                    exact_writer?1u:0u,
                    exact_writer?name.data():"NONE",
                    listed?1u:0u);
                reshade::log::message(reshade::log::level::info,line);
            }
            stock_resource->Release();
        }
    }
#endif
"""
    s=replace_once(s,anchor,anchor+diag)
    return s

def main() -> None:
    ap=argparse.ArgumentParser()
    ap.add_argument("--apply",action="store_true")
    args=ap.parse_args()
    transport(args.apply)
    before=MAT.read_text(encoding="utf-8").replace("\r\n","\n")
    if "stage=nonmetal_resource_probe" not in before:
        raise AssertionError("V205 requires exact PR306 resource parent")
    after=material(before)
    if args.apply and after!=before:
        MAT.write_text(after,encoding="utf-8",newline="\n")
    elif not args.apply and after!=before:
        raise AssertionError("V205 material runtime source overlay missing")
    print("V205_SPC25_EXACT_CPU_NATIVE_SRV_EPOCH_SOURCE_"+(
        "APPLIED" if args.apply else "VERIFIED"))
    print("V205_SPC25_ALL_RX24_TO_35_READ_ONLY=ON")
    print("V205_GSPEC_T10_CARRIER_UNCHANGED=ON")
    print("V205_PMETAL_ROUTE345_UNCHANGED=ON")

if __name__=="__main__":
    main()
