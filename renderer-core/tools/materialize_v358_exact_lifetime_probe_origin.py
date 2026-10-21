#!/usr/bin/env python3
"""V358: exact BC6H producer-to-rejected-t12 join, bound to native COM lifetime.
No donor admission, owner guess, resource copying or draw-time mutation.
Must run after V357 materializer, before CMake. All old guards unchanged.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def replace(path,old,new,label):
    file=root/path
    s=file.read_text("utf8")
    n=s.count(old)
    if n!=1:
        raise SystemExit(f"V358 {label} expected exactly one anchor got {n}")
    file.write_text(s.replace(old,new,1),"utf8")

header="include/dsrrl/runtime/envspec_resource_runtime.hpp"
insert=r'''#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)
struct v358_probe_origin_snapshot {
    std::uint64_t resource_key = 0u;
    std::uint64_t lifetime_serial = 0u;
    std::array<std::uint8_t, 32> creation_sha256{};
    bool initial_data_exact = false;
    bool certified_native_probe = false;
    std::uint16_t certified_probe = 0u;
};
// Read-only; native view MUST reference same live ID3D11Resource on current
// device. Unknown SHA and unknown assets are never granted a PTDE probe.
bool v358_probe_origin_for_native_view(
    ID3D11ShaderResourceView *view,
    v358_probe_origin_snapshot &out) noexcept;
#endif

'''
replace(header,"namespace dsrrl::runtime {\n",
        "namespace dsrrl::runtime {\n"+insert,"header API")

res="src/runtime/envspec_resource_runtime.cpp"
record=r'''
#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)
struct v358_lifetime_probe_record {
    std::uint64_t serial = 0u;
    std::array<std::uint8_t,32> sha{};
    bool exact_data = false;
    bool certified = false;
    std::uint16_t probe = 0u;
};
// A resource pointer is only a key while its resource lifetime is active.
// Destroy erases it, reallocation gains a new monotonically increasing serial.
std::unordered_map<std::uint64_t,v358_lifetime_probe_record>
    g_v358_lifetime_probes;
std::atomic<std::uint64_t> g_v358_lifetime_next{1u};
#endif
'''
replace(res,"std::unordered_map<std::uint64_t,native_probe_record>\n    g_native_resources;",
        "std::unordered_map<std::uint64_t,native_probe_record>\n    g_native_resources;"+record,"record map")
replace(res,"        g_native_resources.clear();\n        g_resource_by_view.clear();",
'''        g_native_resources.clear();
#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)
        g_v358_lifetime_probes.clear();
#endif
        g_resource_by_view.clear();''',"device cleanup")
replace(res,"        g_native_resources.reserve(\n            env::k_legacy_envspec_probe_count);",
'''        g_native_resources.reserve(
            env::k_legacy_envspec_probe_count);
#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)
        g_v358_lifetime_probes.reserve(512u);
#endif''',"init reserve")
# V359 has two independent BC6H producer families. V358's historical
# EnvSpec origin census only owns 256px t12/t14, never diffuse 64px t11/t13.
replace(res,'''    if (!extract_tight_bc6h(desc, initial_data, tight)) {''',
'''    if (!extract_tight_bc6h(desc, initial_data, tight)) {
#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)
        if (spec) {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (device_ptr==g_device) {
                v358_lifetime_probe_record item{};
                item.serial=g_v358_lifetime_next.fetch_add(
                    1u,std::memory_order_relaxed);
                g_v358_lifetime_probes[resource_handle.handle]=item;
            }
        }
#endif''',"missing BC6H initial data")
replace(res,'''    if (!probe.has_value()) {
        if (spec) {''',
'''#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)
    if (spec) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (device_ptr==g_device) {
            v358_lifetime_probe_record item{};
            item.serial=g_v358_lifetime_next.fetch_add(
                1u,std::memory_order_relaxed);
            item.sha=digest;
            item.exact_data=true;
            item.certified=probe.has_value();
            item.probe=probe.has_value() ? probe.value() : 0u;
            g_v358_lifetime_probes[resource_handle.handle]=item;
        }
    }
#endif
    if (!probe.has_value()) {
        if (spec) {''',"certified and unknown native creation SHA")
replace(res,'''    g_native_resources.erase(
        resource_handle.handle);
    g_native_diffuse_resources.erase(resource_handle.handle);
    g_diffuse_unverified_views.clear();

    g_snapshot_epoch.fetch_add(''',
'''    g_native_resources.erase(
        resource_handle.handle);
#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)
    g_v358_lifetime_probes.erase(resource_handle.handle);
#endif
    g_native_diffuse_resources.erase(resource_handle.handle);
    g_diffuse_unverified_views.clear();

    g_snapshot_epoch.fetch_add(''',"resource lifetime retirement")
helper=r'''#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)
bool v358_probe_origin_for_native_view(
    ID3D11ShaderResourceView *view,
    v358_probe_origin_snapshot &out) noexcept
{
    out={};
    if(view==nullptr) return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
    view->GetDesc(&desc);
    if(desc.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURECUBE ||
       (desc.Format!=DXGI_FORMAT_BC6H_UF16 &&
        desc.Format!=DXGI_FORMAT_BC6H_SF16))
        return false;
    ID3D11Resource *resource=nullptr;
    ID3D11Device *native_device=nullptr;
    view->GetResource(&resource);
    view->GetDevice(&native_device);
    if(resource==nullptr || native_device==nullptr) {
        if(resource!=nullptr) resource->Release();
        if(native_device!=nullptr) native_device->Release();
        return false;
    }
    const auto key=static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(resource));
    bool matched=false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if(g_device!=nullptr &&
            reinterpret_cast<std::uintptr_t>(native_device)==
            static_cast<std::uintptr_t>(g_device->get_native())) {
            const auto it=g_v358_lifetime_probes.find(key);
            if(it!=g_v358_lifetime_probes.end()) {
                const auto &origin=it->second;
                out.resource_key=key;
                out.lifetime_serial=origin.serial;
                out.creation_sha256=origin.sha;
                out.initial_data_exact=origin.exact_data;
                out.certified_native_probe=origin.certified;
                out.certified_probe=origin.probe;
                matched=true;
            }
        }
    }
    native_device->Release();
    resource->Release();
    return matched;
}
#endif

'''
replace(res,"envspec_resource_runtime::~envspec_resource_runtime()",
        helper+"envspec_resource_runtime::~envspec_resource_runtime()","exact native view origin lookup")

draw="src/runtime/pmetal_envspec_draw_runtime.cpp"
helper=r'''#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)
void v358_log_probe_reject_native_origin(
    ID3D11ShaderResourceView *view,
    std::uint32_t rx,
    std::uint32_t route,
    std::uint32_t slot,
    std::uint32_t binding) noexcept
{
    v358_probe_origin_snapshot origin{};
    const bool joined=
        v358_probe_origin_for_native_view(view,origin);
    char sha[65]{};
    constexpr char hexdigits[]="0123456789abcdef";
    if(joined && origin.initial_data_exact) {
        for(std::size_t i=0u;i<origin.creation_sha256.size();++i) {
            const auto byte=origin.creation_sha256[i];
            sha[2u*i]=hexdigits[byte>>4u];
            sha[2u*i+1u]=hexdigits[byte&15u];
        }
    }
    char line[560]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V358 PROBE ORIGIN]"
        " rx=%u route=%u slot=%u t=%u"
        " live_creation_join=%u resource=%016llx lifetime=%llu"
        " sha_exact=%u sha256=%s certified=%u probe=%u"
        " action=STOCK_DSR_IF_UNAUTHORIZED pixel=OPEN",
        rx,route,slot,binding,joined?1u:0u,
        static_cast<unsigned long long>(origin.resource_key),
        static_cast<unsigned long long>(origin.lifetime_serial),
        joined && origin.initial_data_exact?1u:0u,
        joined && origin.initial_data_exact?sha:"UNAVAILABLE",
        joined && origin.certified_native_probe?1u:0u,
        static_cast<unsigned>(origin.certified_probe));
    reshade::log::message(reshade::log::level::info,line);
}
#endif

'''
replace(draw,"std::atomic<std::uint32_t> g_value_cut_log_mask{0u};",
        "std::atomic<std::uint32_t> g_value_cut_log_mask{0u};\n"+helper,
        "probe reject logger")
file=root/draw
s=file.read_text("utf8")
idx=s.index("[DSRRL V354 PROBE REJECT]")
target="            reshade::log::message(reshade::log::level::info,line);"
at=s.find(target,idx)
if at<0 or at-idx>2200:
    raise SystemExit("V358 actual V354 PROBE REJECT log call not found")
replacement=target+'''
#if defined(DSRRL_V358_EXACT_PROBE_ORIGIN)
            // At the actual rejected first consumer, while SRVs are live.
            // Corrects V357's census which only ran beside SPEC REJECT.
            v358_log_probe_reject_native_origin(
                native_view[0],
                decision.receiver_id,decision.route_index,
                static_cast<unsigned>(env_semantics.envspc_slot),12u);
            if (probe_b_required)
                v358_log_probe_reject_native_origin(
                    native_view[2],
                    decision.receiver_id,decision.route_index,
                    static_cast<unsigned>(env_semantics.envspc_slot),14u);
#endif'''
s=s[:at]+s[at:].replace(target,replacement,1)
file.write_text(s,"utf8")

print("V358_BC6H_EXACT_LIFETIME_SOURCE_TO_ACTUAL_PROBE_REJECT_PATCH_PASS")
