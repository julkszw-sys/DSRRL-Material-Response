#!/usr/bin/env python3
"""V364 stage B: use certified native legacy t11 selector to choose
the corresponding *original PTDE* DDS donor, never EnvSpec selector.
Does not relax any receiver or EnvSpec transaction gates.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"src/runtime/envspec_resource_runtime.cpp"
s=p.read_text("utf8")
def once(a,b,tag):
    global s
    count=s.count(a)
    if count!=1:raise SystemExit("V364 B "+tag+" anchor drift count="+str(count))
    s=s.replace(a,b,1)

helper=r'''
bool get_v364_ptde_legacy_envdiffuse_cube(
    std::uint16_t selector,resource_view &view) noexcept
{
    view={};
#if !defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    (void)selector;
    return false;
#else
    const auto *entry=operators::env_spec::v364_legacy_by_selector(selector);
    if(!entry)return false;
    const unsigned index=selector==500u?0u:1u;
    device *device_ptr=nullptr;
    std::vector<std::uint8_t> bytes;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if(g_device==nullptr || !g_sampler_ready ||
           g_v364_legacy_rgbas[index].size()!=entry->bytes)
            return false;
        device_ptr=g_device;
        const auto existing=g_ptde_envdiffuse_cubes.find(selector);
        if(existing!=g_ptde_envdiffuse_cubes.end()) {
            view=existing->second.view;
            return view.handle!=0u;
        }
        bytes=g_v364_legacy_rgbas[index];
    }
    const unsigned face_bytes=entry->edge*entry->edge*4u;
    std::array<subresource_data,k_faces> subresources{};
    for(unsigned face=0u;face<k_faces;++face) {
        subresources[face].data=bytes.data()+face*face_bytes;
        subresources[face].row_pitch=entry->edge*4u;
        subresources[face].slice_pitch=face_bytes;
    }
    const resource_desc desc(
        resource_type::texture_2d,entry->edge,entry->edge,k_faces,1u,
        format::r8g8b8a8_unorm,1u,memory_heap::default_,
        resource_usage::shader_resource,resource_flags::cube_compatible);
    rgba_cube cube{};
    if(!device_ptr->create_resource(
        desc,subresources.data(),resource_usage::shader_resource,
        &cube.texture))return false;
    const resource_view_desc view_desc(
        resource_view_type::texture_cube,format::r8g8b8a8_unorm,
        0u,1u,0u,k_faces);
    if(!device_ptr->create_resource_view(
        cube.texture,resource_usage::shader_resource,view_desc,
        &cube.view)) {
        device_ptr->destroy_resource(cube.texture);
        return false;
    }
    bool still_alive=false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        still_alive=g_device==device_ptr &&
            g_v364_legacy_rgbas[index].size()==entry->bytes;
        if(still_alive) {
            const auto result=g_ptde_envdiffuse_cubes.emplace(
                selector,cube);
            view=result.first->second.view;
            if(result.second) {
                if(g_v364_legacy_prepare_logged.fetch_or(
                    1u<<index,std::memory_order_relaxed)==0u) {
                    char line[240]{};
                    std::snprintf(line,sizeof(line),
                        "[DSRRL V364 PTDE LEGACY ENVDIFFUSE CUBE]"
                        " name=%s selector=%u edge=%u"
                        " carrier=EXACT_ORIGINAL_PTDE_RGBA"
                        " envspec=UNCHANGED pixel=OPEN",
                        entry->dsr_name,
                        static_cast<unsigned>(selector),entry->edge);
                    reshade::log::message(reshade::log::level::info,line);
                }
            } else {
                still_alive=false; // discard duplicate cube, return existing
            }
        }
    }
    if(!still_alive) {
        device_ptr->destroy_resource_view(cube.view);
        device_ptr->destroy_resource(cube.texture);
    }
    return view.handle!=0u;
#endif
}

'''
once('bool get_ptde_envdiffuse_cube(\n',helper+'bool get_ptde_envdiffuse_cube(\n',
    "isolated original PTDE legacy cube GPU carrier")

once('''    if (probe >=
            env::k_legacy_envspec_probe_count)
        return false;

    device *device_ptr = nullptr;
    std::array<std::uint8_t,k_ptde_cube_bytes>
        cube_bytes{};''',
'''    if(operators::env_spec::v364_legacy_by_selector(probe)!=nullptr)
        return get_v364_ptde_legacy_envdiffuse_cube(probe,view);
    if (probe >=
            env::k_legacy_envspec_probe_count)
        return false;

    device *device_ptr = nullptr;
    std::array<std::uint8_t,k_ptde_cube_bytes>
        cube_bytes{};''',"separate physical donor class")

once('''    if (!native_envdiffuse_probe_for_view(stock_t11,a) ||
        (probe_b_required &&
         !native_envdiffuse_probe_for_view(stock_t13,b)))
        return false;
    if (!probe_b_required) b=a;
    return prepare_envdiffuse(a,b,probe_b_required,prepared);''',
'''    const auto independent_probe=[this](
        ID3D11ShaderResourceView *view,
        std::uint16_t &selected) noexcept {
        if(native_envdiffuse_probe_for_view(view,selected))
            return true; // existing exact PackedGI producer
        return v364_exact_legacy_selector_for_view(view,selected);
    };
    if(!independent_probe(stock_t11,a) ||
       (probe_b_required && !independent_probe(stock_t13,b)))
        return false;
    if(!probe_b_required)b=a;
    // Never mix legacy and PackedGI endpoints within an unverified LERP.
    if(probe_b_required &&
       ((a>=env::k_legacy_envspec_probe_count) !=
        (b>=env::k_legacy_envspec_probe_count)))
        return false;
    return prepare_envdiffuse(a,b,probe_b_required,prepared);''',
    "independent A/B selectors no t12/t14 source")

# Only patch the EnvDiffuse preparation method: EnvSpec itself has a
# similar 342-record guard and MUST remain bit-for-bit unchanged.
prefix, diffuse_tail = s.split(
    "bool envspec_resource_runtime::prepare_envdiffuse(",1)
diffuse_body, suffix = diffuse_tail.split(
    "void envspec_resource_runtime::release(",1)
old='''    if (probe_a >=
            env::k_legacy_envspec_probe_count ||
        probe_b >=
            env::k_legacy_envspec_probe_count)
        return false;

    resource_view a_view{};'''
new='''    const auto known=[](std::uint16_t id) noexcept {
        return id<env::k_legacy_envspec_probe_count ||
            operators::env_spec::v364_legacy_by_selector(id)!=nullptr;
    };
    if(!known(probe_a) || !known(probe_b))
        return false;

    resource_view a_view{};'''
if diffuse_body.count(old)!=1:
    raise SystemExit("V364 EnvDiffuse scoped ordinal gate drift")
diffuse_body=diffuse_body.replace(old,new,1)
s=prefix+"bool envspec_resource_runtime::prepare_envdiffuse("+
    diffuse_body+"void envspec_resource_runtime::release("+suffix
p.write_text(s,"utf8")
print("V364 independent legacy/packed t11 t13 -> exact PTDE carrier patch PASS")
