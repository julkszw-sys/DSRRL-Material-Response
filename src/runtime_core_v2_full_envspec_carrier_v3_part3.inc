            const std::uint32_t sub = face * k_dsr_levels + level;
            const auto &src = initial_data[sub];
            if (src.data == nullptr) return false;
            const std::uint32_t width = std::max(1u, k_dsr_size >> level);
            const std::uint32_t height = std::max(1u, k_dsr_size >> level);
            const std::uint32_t block_cols = std::max(1u, (width + 3u) / 4u);
            const std::uint32_t block_rows = std::max(1u, (height + 3u) / 4u);
            const std::uint32_t tight_row = block_cols * 16u;
            const std::uint32_t row_pitch = src.row_pitch != 0 ? src.row_pitch : tight_row;
            if (row_pitch < tight_row) return false;
            const auto *bytes = static_cast<const std::uint8_t *>(src.data);
            for (std::uint32_t y = 0; y < block_rows; ++y)
                out.insert(out.end(), bytes + static_cast<std::size_t>(y) * row_pitch,
                           bytes + static_cast<std::size_t>(y) * row_pitch + tight_row);
        }
    }
    return out.size() == k_dsr_tight_bc6h_bytes;
}

struct float3 { float r, g, b; };

std::uint32_t encode_ufloat(float value, std::uint32_t mantissa_bits)
{
    if (!(value > 0.0f)) return 0;
    if (!std::isfinite(value)) return (31u << mantissa_bits);
    int exponent = 0;
    const float fraction = std::frexp(value, &exponent);
    int target_exponent = exponent - 1 + 15;
    const std::uint32_t scale = 1u << mantissa_bits;
    if (target_exponent >= 31) return (30u << mantissa_bits) | (scale - 1u);
    if (target_exponent <= 0) {
        const long rounded = std::lround(std::ldexp(value, 14 + static_cast<int>(mantissa_bits)));
        if (rounded <= 0) return 0;
        if (rounded >= static_cast<long>(scale)) return 1u << mantissa_bits;
        return static_cast<std::uint32_t>(rounded);
    }
    long mantissa = std::lround((fraction * 2.0f - 1.0f) * static_cast<float>(scale));
    if (mantissa >= static_cast<long>(scale)) {
        mantissa = 0; ++target_exponent;
        if (target_exponent >= 31) return (30u << mantissa_bits) | (scale - 1u);
    }
    return (static_cast<std::uint32_t>(target_exponent) << mantissa_bits) |
           static_cast<std::uint32_t>(mantissa);
}

std::uint32_t pack_r11g11b10(const float3 &v)
{
    return encode_ufloat(v.r, 6) |
          (encode_ufloat(v.g, 6) << 11) |
          (encode_ufloat(v.b, 5) << 22);
}

bool create_black_cube()
{
    if (g_device == nullptr) return false;
    if (g_black_view.handle != 0) return true;
    std::array<std::uint32_t,k_faces> zeros{};
    std::array<reshade::api::subresource_data,k_faces> sub{};
    for (std::uint32_t f = 0; f < k_faces; ++f) {
        sub[f].data = &zeros[f]; sub[f].row_pitch = 4; sub[f].slice_pitch = 4;
    }
    const reshade::api::resource_desc desc(
        reshade::api::resource_type::texture_2d, 1, 1, k_faces, 1,
        reshade::api::format::r11g11b10_float, 1,
        reshade::api::memory_heap::default_, reshade::api::resource_usage::shader_resource,
        reshade::api::resource_flags::cube_compatible);
    g_internal_resource_create = true;
    const bool resource_ok = g_device->create_resource(desc, sub.data(),
        reshade::api::resource_usage::shader_resource, &g_black_resource);
    bool view_ok = false;
    if (resource_ok) {
        const reshade::api::resource_view_desc vd(
            reshade::api::resource_view_type::texture_cube,
            reshade::api::format::r11g11b10_float, 0, 1, 0, k_faces);
        view_ok = g_device->create_resource_view(g_black_resource,
            reshade::api::resource_usage::shader_resource, vd, &g_black_view);
    }
    if (!view_ok && g_black_resource.handle != 0) g_device->destroy_resource(g_black_resource);
    g_internal_resource_create = false;
    return resource_ok && view_ok;
}

std::optional<reshade::api::resource_view> get_virtual_cube(std::uint16_t probe, std::uint8_t slot)
{
    if (g_device == nullptr || g_ptde_pack.empty() || probe >= k_probe_count || slot >= k_ptde_slots)
        return std::nullopt;
    const std::uint32_t key = static_cast<std::uint32_t>(probe) * k_ptde_slots + slot;
    {
        std::lock_guard lock(g_mutex);
        const auto it = g_virtual_cubes.find(key);
        if (it != g_virtual_cubes.end()) return it->second.view;
    }
    const std::size_t offset = static_cast<std::size_t>(key) * k_ptde_bytes_per_cube;
    std::array<std::vector<std::uint32_t>,k_faces> storage;
    std::array<reshade::api::subresource_data,k_faces> sub{};
    const std::size_t face_bytes = static_cast<std::size_t>(k_ptde_size) * k_ptde_size * 4u;
    for (std::uint32_t face = 0; face < k_faces; ++face) {
        auto &pixels = storage[face]; pixels.resize(k_ptde_size * k_ptde_size);
        const auto *src = g_ptde_pack.data() + offset + face * face_bytes;
        for (std::uint32_t i = 0; i < k_ptde_size * k_ptde_size; ++i) {
            const std::uint8_t a8 = src[i * 4u + 3u];
            const float denom = static_cast<float>(std::max<std::uint8_t>(a8, 1u));
            const float3 v{
                static_cast<float>(src[i * 4u + 0u]) / denom,
                static_cast<float>(src[i * 4u + 1u]) / denom,
                static_cast<float>(src[i * 4u + 2u]) / denom};
            pixels[i] = pack_r11g11b10(v);
        }
        sub[face].data = pixels.data();
        sub[face].row_pitch = k_ptde_size * sizeof(std::uint32_t);
        sub[face].slice_pitch = k_ptde_size * k_ptde_size * sizeof(std::uint32_t);
    }
    const reshade::api::resource_desc desc(
        reshade::api::resource_type::texture_2d, k_ptde_size, k_ptde_size, k_faces, 1,
        reshade::api::format::r11g11b10_float, 1,
        reshade::api::memory_heap::default_, reshade::api::resource_usage::shader_resource,
        reshade::api::resource_flags::cube_compatible);
    virtual_cube cube{}; cube.probe_index = probe; cube.slot = slot;
    g_internal_resource_create = true;
    const bool resource_ok = g_device->create_resource(desc, sub.data(),
        reshade::api::resource_usage::shader_resource, &cube.resource);
    bool view_ok = false;
    if (resource_ok) {
        const reshade::api::resource_view_desc vd(
            reshade::api::resource_view_type::texture_cube,
            reshade::api::format::r11g11b10_float, 0, 1, 0, k_faces);
        view_ok = g_device->create_resource_view(cube.resource,
            reshade::api::resource_usage::shader_resource, vd, &cube.view);
    }
    if (!view_ok && cube.resource.handle != 0) g_device->destroy_resource(cube.resource);
    g_internal_resource_create = false;
    if (!resource_ok || !view_ok) { ++g_virtual_create_fail; return std::nullopt; }
    {
        std::lock_guard lock(g_mutex);
        const auto [it, inserted] = g_virtual_cubes.emplace(key, cube);
        if (!inserted) {
            g_internal_resource_create = true;
            g_device->destroy_resource_view(cube.view);
            g_device->destroy_resource(cube.resource);
            g_internal_resource_create = false;
            return it->second.view;
        }
    }
    ++g_virtual_created;
    return cube.view;
}

void on_init_device(reshade::api::device *device)
{
    if (g_device != nullptr && g_device != device) return;
    g_device = device;
    reshade::api::sampler_desc sd{};
    sd.filter = reshade::api::filter_mode::anisotropic;
    sd.address_u = reshade::api::texture_address_mode::mirror;
    sd.address_v = reshade::api::texture_address_mode::mirror;
    sd.address_w = reshade::api::texture_address_mode::wrap;
    sd.max_anisotropy = 1.0f;
    sd.min_lod = 0.0f;
    sd.max_lod = 0.0f;
    device->create_sampler(sd, &g_ptde_sampler);
    create_black_cube();
}

void on_destroy_device(reshade::api::device *device)
{
    if (g_device != device) return;
    g_internal_resource_create = true;
    for (const auto &[_, cube] : g_virtual_cubes) {
        if (cube.view.handle != 0) device->destroy_resource_view(cube.view);
        if (cube.resource.handle != 0) device->destroy_resource(cube.resource);
    }
    g_virtual_cubes.clear();
    if (g_black_view.handle != 0) device->destroy_resource_view(g_black_view);
    if (g_black_resource.handle != 0) device->destroy_resource(g_black_resource);
    if (g_ptde_sampler.handle != 0) device->destroy_sampler(g_ptde_sampler);
    g_internal_resource_create = false;
    g_black_view = {}; g_black_resource = {}; g_ptde_sampler = {}; g_device = nullptr;
}

void on_init_command_list(reshade::api::command_list *cmd)
{
    std::lock_guard lock(g_mutex);
    g_command_bindings.try_emplace(command_id(cmd));
}

void on_destroy_command_list(reshade::api::command_list *cmd)
{
    std::lock_guard lock(g_mutex);
    g_command_bindings.erase(command_id(cmd));
}

void on_init_resource(reshade::api::device *, const reshade::api::resource_desc &desc,
                      const reshade::api::subresource_data *initial_data,
                      reshade::api::resource_usage, reshade::api::resource resource)
{
    if (g_internal_resource_create || resource.handle == 0 || !is_native_envspec_desc(desc)) return;
    ++g_native_candidates;
    std::vector<std::uint8_t> tight;
    if (!extract_tight_bc6h(desc, initial_data, tight)) { ++g_native_hash_miss; return; }
    std::array<std::uint8_t,32> sha{};
    if (!sha256_bytes(tight.data(), tight.size(), sha)) { ++g_native_hash_miss; return; }
    const auto probe = find_native_probe(sha);
    if (!probe.has_value()) { ++g_native_hash_miss; return; }
    {
        std::lock_guard lock(g_mutex);
        g_native_resources[resource.handle] = native_resource_record{*probe};
    }
    ++g_native_matches;
}

void on_destroy_resource(reshade::api::device *, reshade::api::resource resource)
{
    if (g_internal_resource_create || resource.handle == 0) return;
    std::lock_guard lock(g_mutex);
    g_native_resources.erase(resource.handle);
    for (auto it = g_source_resource_by_view.begin(); it != g_source_resource_by_view.end();) {
        if (it->second == resource.handle) it = g_source_resource_by_view.erase(it); else ++it;
    }
}

void on_init_resource_view(reshade::api::device *, reshade::api::resource resource,
                           reshade::api::resource_usage usage,
                           const reshade::api::resource_view_desc &,
                           reshade::api::resource_view view)
{
    if (g_internal_resource_create || resource.handle == 0 || view.handle == 0 ||
        (usage & reshade::api::resource_usage::shader_resource) == reshade::api::resource_usage::undefined) return;
    std::lock_guard lock(g_mutex);
    if (g_native_resources.find(resource.handle) != g_native_resources.end())
        g_source_resource_by_view[view.handle] = resource.handle;
}

void on_destroy_resource_view(reshade::api::device *, reshade::api::resource_view view)
{
    if (g_internal_resource_create || view.handle == 0) return;
    std::lock_guard lock(g_mutex);
    g_source_resource_by_view.erase(view.handle);
}

int tracked_slot(std::uint32_t binding) noexcept
{
    return binding == 12 ? 0 : (binding == 14 ? 1 : -1);
}

void on_push_descriptors(reshade::api::command_list *cmd,
                         reshade::api::shader_stage stages,
                         reshade::api::pipeline_layout layout,
                         std::uint32_t param_index,
                         const reshade::api::descriptor_table_update &update)
{
    if (g_internal_descriptor_push ||
        (stages & reshade::api::shader_stage::pixel) != reshade::api::shader_stage::pixel ||
        update.descriptors == nullptr) return;
    const std::uint64_t id = command_id(cmd);
    std::lock_guard lock(g_mutex);
    auto &state = g_command_bindings[id];
    for (std::uint32_t i = 0; i < update.count; ++i) {
        const int s = tracked_slot(update.binding + i);
        if (s < 0) continue;
        if (update.type == reshade::api::descriptor_type::shader_resource_view) {
            const auto *v = static_cast<const reshade::api::resource_view *>(update.descriptors);
            state.srv[s] = srv_binding{true, layout, param_index, update.type, v[i], {}};
        } else if (update.type == reshade::api::descriptor_type::sampler_with_resource_view) {
            const auto *v = static_cast<const reshade::api::sampler_with_resource_view *>(update.descriptors);
            state.srv[s] = srv_binding{true, layout, param_index, update.type, v[i].view, v[i].sampler};
        } else if (update.type == reshade::api::descriptor_type::sampler) {
            const auto *v = static_cast<const reshade::api::sampler *>(update.descriptors);
            state.sampler[s] = sampler_binding{true, layout, param_index, v[i]};
        }
    }
}

std::optional<material_binding> active_binding()
{
    if (g_active_material == 0) return std::nullopt;
    std::lock_guard lock(g_mutex);
    const auto it = g_materials.find(g_active_material);
    if (it == g_materials.end()) return std::nullopt;
    return it->second;
}

std::optional<std::uint16_t> probe_for_view(reshade::api::resource_view view)
{
    if (view.handle == 0) return std::nullopt;
    std::lock_guard lock(g_mutex);
    const auto vit = g_source_resource_by_view.find(view.handle);
    if (vit == g_source_resource_by_view.end()) return std::nullopt;
    const auto rit = g_native_resources.find(vit->second);
    if (rit == g_native_resources.end()) return std::nullopt;
    return rit->second.probe_index;
