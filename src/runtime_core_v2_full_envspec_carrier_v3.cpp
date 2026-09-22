#define AddonInit V31InternalAddonInit
#define AddonUninit V31InternalAddonUninit
#define NAME V31_INTERNAL_NAME
#define DESCRIPTION V31_INTERNAL_DESCRIPTION
#include "runtime_core_v2_full_envspec_carrier_v3_part1.inc"
#include "runtime_core_v2_full_envspec_carrier_v3_part2.inc"
#include "runtime_core_v2_full_envspec_carrier_v3_part3.inc"
#include "runtime_core_v2_full_envspec_carrier_v3_part4.inc"
#undef DESCRIPTION
#undef NAME
#undef AddonUninit
#undef AddonInit

namespace dsrrl::runtime_v2::full_envspec_v3 {
namespace {

// V3.3 keeps the V3.2 callsite coexistence architecture so Material Response
// can retain ownership of its parser/selector entry hooks. Raw PTDE RGBA is
// intentionally preserved through texture filtering and decoded by build131.
constexpr std::uintptr_t k_parser_thunk_rva = 0x29DFC0;
constexpr std::array<std::uintptr_t, 4> k_compat_call_rvas = {
    0x291D9C, 0x20E014, 0x20EB7A, 0x20FB99
};
constexpr std::array<std::array<std::uint8_t,5>, 4> k_compat_expected_calls = {{
    {{0xE8,0x1F,0xC2,0x00,0x00}},
    {{0xE8,0x07,0xDA,0x01,0x00}},
    {{0xE8,0xA1,0xCE,0x01,0x00}},
    {{0xE8,0x82,0xBE,0x01,0x00}}
}};

struct callsite_patch {
    std::uint8_t *site = nullptr;
    std::array<std::uint8_t,5> original{};
    std::uint8_t *relay = nullptr;
    bool installed = false;
};

std::array<callsite_patch,4> g_compat_patches{};
std::uint8_t *g_relay_page = nullptr;
constexpr std::size_t k_relay_page_size = 4096;
constexpr std::size_t k_relay_stride = 16;

using parser_thunk_fn = void (__fastcall *)(void *, const void *, std::uint32_t);
parser_thunk_fn g_parser_thunk_original = nullptr;
selector_fn g_selector_entry = nullptr;

void *alloc_near_page(void *near_address)
{
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const std::uintptr_t gran = static_cast<std::uintptr_t>(si.dwAllocationGranularity);
    const std::uintptr_t anchor = reinterpret_cast<std::uintptr_t>(near_address) & ~(gran - 1u);
    const std::uintptr_t min_addr = reinterpret_cast<std::uintptr_t>(si.lpMinimumApplicationAddress);
    const std::uintptr_t max_addr = reinterpret_cast<std::uintptr_t>(si.lpMaximumApplicationAddress);
    constexpr std::uintptr_t limit = 0x7FFF0000ull;

    for (std::uintptr_t delta = 0; delta <= limit; delta += gran) {
        if (anchor >= delta) {
            const std::uintptr_t down = anchor - delta;
            if (down >= min_addr) {
                if (void *p = VirtualAlloc(reinterpret_cast<void *>(down), k_relay_page_size,
                                           MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))
                    return p;
            }
        }
        if (delta != 0 && anchor <= max_addr - delta) {
            const std::uintptr_t up = anchor + delta;
            if (up <= max_addr) {
                if (void *p = VirtualAlloc(reinterpret_cast<void *>(up), k_relay_page_size,
                                           MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))
                    return p;
            }
        }
    }
    return nullptr;
}

void write_abs_relay(std::uint8_t *dst, const void *target)
{
    dst[0] = 0xFF; dst[1] = 0x25;
    dst[2] = dst[3] = dst[4] = dst[5] = 0;
    const auto address = reinterpret_cast<std::uint64_t>(target);
    std::memcpy(dst + 6, &address, sizeof(address));
    dst[14] = 0x90; dst[15] = 0x90;
}

bool patch_callsite(std::size_t index, std::uint8_t *site, const void *hook,
                    const std::array<std::uint8_t,5> &expected)
{
    if (index >= g_compat_patches.size() || site == nullptr || hook == nullptr || g_relay_page == nullptr)
        return false;
    if (std::memcmp(site, expected.data(), expected.size()) != 0)
        return false;

    auto &state = g_compat_patches[index];
    state.site = site;
    state.original = expected;
    state.relay = g_relay_page + index * k_relay_stride;
    write_abs_relay(state.relay, hook);

    const std::intptr_t displacement = state.relay - (site + 5);
    if (displacement < std::numeric_limits<std::int32_t>::min() ||
        displacement > std::numeric_limits<std::int32_t>::max())
        return false;

    DWORD old_protect = 0;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old_protect))
        return false;
    std::array<std::uint8_t,5> patch{};
    patch[0] = 0xE8;
    const auto rel = static_cast<std::int32_t>(displacement);
    std::memcpy(patch.data() + 1, &rel, sizeof(rel));
    std::memcpy(site, patch.data(), patch.size());
    FlushInstructionCache(GetCurrentProcess(), site, patch.size());
    DWORD ignored = 0;
    VirtualProtect(site, 5, old_protect, &ignored);
    state.installed = true;
    return true;
}

void unpatch_callsites()
{
    for (auto &state : g_compat_patches) {
        if (!state.installed || state.site == nullptr) continue;
        DWORD old_protect = 0;
        if (VirtualProtect(state.site, state.original.size(), PAGE_EXECUTE_READWRITE, &old_protect)) {
            std::memcpy(state.site, state.original.data(), state.original.size());
            FlushInstructionCache(GetCurrentProcess(), state.site, state.original.size());
            DWORD ignored = 0;
            VirtualProtect(state.site, state.original.size(), old_protect, &ignored);
        }
        state = {};
    }
    if (g_relay_page != nullptr) {
        VirtualFree(g_relay_page, 0, MEM_RELEASE);
        g_relay_page = nullptr;
    }
    g_parser_thunk_original = nullptr;
    g_selector_entry = nullptr;
}

void remember_material_binding_v33(void *material, const void *raw, std::uint32_t size,
                                   const wchar_t *semantic_key)
{
    material_binding binding{};
    if (material != nullptr && raw != nullptr && size != 0 && size <= (1u << 20)) {
        std::array<std::uint8_t,32> sha{};
        const std::uint64_t name_hash = fnv1a_utf16_basename_lower_ascii(semantic_key);
        if (sha256_bytes(raw, size, sha)) {
            binding.record = find_material(name_hash, sha);
            if (binding.record != nullptr &&
                binding.record->state == material_envspec_state::explicit_none)
                binding.explicit_none_safe = binding.record->explicit_none_safe;
        }
    }
    if (material != nullptr) {
        std::lock_guard lock(g_mutex);
        if (binding.record != nullptr) {
            g_materials[reinterpret_cast<std::uintptr_t>(material)] = binding;
            ++g_material_mapped;
        } else {
            g_materials.erase(reinterpret_cast<std::uintptr_t>(material));
            ++g_material_unmapped;
        }
    }
}

void __fastcall parser_callsite_hook_v33(void *owner, const void *raw, std::uint32_t size)
{
    ++g_parser_seen;
    const wchar_t *semantic_key = nullptr;
    void *material = nullptr;
    if (owner != nullptr) {
        __try {
            semantic_key = *reinterpret_cast<const wchar_t *const *>(
                static_cast<const std::uint8_t *>(owner) + 0x08);
            material = static_cast<std::uint8_t *>(owner) + 0x28;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            semantic_key = nullptr;
            material = nullptr;
        }
    }
    if (g_parser_thunk_original != nullptr)
        g_parser_thunk_original(owner, raw, size);
    remember_material_binding_v33(material, raw, size, semantic_key);
}

std::uintptr_t __fastcall selector_callsite_hook_v33(
    void *container, void *arg2, std::int32_t material_index, std::uint32_t arg4,
    std::uint64_t arg5, std::uint64_t arg6, std::uint64_t arg7,
    std::uint64_t arg8, std::uint64_t arg9, std::uint64_t arg10)
{
    ++g_selector_seen;
    std::uintptr_t actual = 0;
    if (container != nullptr && material_index >= 0) {
        __try {
            const auto base = *reinterpret_cast<const std::uintptr_t *>(
                static_cast<const std::uint8_t *>(container) + 0x10);
            if (base != 0)
                actual = *reinterpret_cast<const std::uintptr_t *>(
                    base + static_cast<std::uintptr_t>(material_index) * 24u);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            actual = 0;
        }
    }
    if (actual != 0) ++g_selector_resolved;
    g_active_material = actual;

    if (g_selector_entry == nullptr) return 0;
    return g_selector_entry(container, arg2, material_index, arg4,
                            arg5, arg6, arg7, arg8, arg9, arg10);
}

bool verify_and_install_compat_hooks_v33()
{
    HMODULE exe = GetModuleHandleW(nullptr);
    if (exe == nullptr) return false;
    std::array<std::uint8_t,32> digest{};
    const auto path = module_path(exe);
    if (path.empty() || !sha256_file(path, digest) ||
        hex_string(digest.data(), digest.size()) != k_expected_exe_sha256) {
        ++g_exe_guard_fail;
        return false;
    }
    ++g_exe_guard_pass;

    auto *base = reinterpret_cast<std::uint8_t *>(exe);
    g_relay_page = static_cast<std::uint8_t *>(alloc_near_page(base + k_compat_call_rvas[0]));
    if (g_relay_page == nullptr) {
        ++g_hook_install_fail;
        return false;
    }

    g_parser_thunk_original = reinterpret_cast<parser_thunk_fn>(base + k_parser_thunk_rva);
    g_selector_entry = reinterpret_cast<selector_fn>(base + k_selector_rva);

    if (!patch_callsite(0, base + k_compat_call_rvas[0],
                        reinterpret_cast<const void *>(&parser_callsite_hook_v33),
                        k_compat_expected_calls[0]) ||
        !patch_callsite(1, base + k_compat_call_rvas[1],
                        reinterpret_cast<const void *>(&selector_callsite_hook_v33),
                        k_compat_expected_calls[1]) ||
        !patch_callsite(2, base + k_compat_call_rvas[2],
                        reinterpret_cast<const void *>(&selector_callsite_hook_v33),
                        k_compat_expected_calls[2]) ||
        !patch_callsite(3, base + k_compat_call_rvas[3],
                        reinterpret_cast<const void *>(&selector_callsite_hook_v33),
                        k_compat_expected_calls[3])) {
        unpatch_callsites();
        ++g_hook_install_fail;
        return false;
    }

    ++g_hook_install_pass;
    return true;
}

struct dedicated_identity {
    std::uint32_t receiver_id;
    std::uint32_t code_size;
    std::array<std::uint8_t,16> checksum;
};

inline constexpr std::array<dedicated_identity,3> k_build131_pmetal_receivers = {{
    {20072u, 19872u, {{0x17,0x17,0x21,0xfc,0x0e,0x2a,0xa0,0x10,0x88,0xb5,0x5f,0xa4,0xa9,0x06,0x7e,0x82}}},
    {20073u, 19572u, {{0x38,0xa2,0x0c,0xac,0xd0,0xf1,0x1e,0x23,0xef,0xb2,0xc2,0xc8,0xf6,0x6b,0x3c,0x91}}},
    {20074u, 18076u, {{0x02,0xb1,0x57,0xdf,0xe6,0x0e,0x24,0xbd,0x07,0xf4,0x05,0xb6,0xd6,0x87,0x47,0x1c}}}
}};

bool classify_receiver_v33(const void *code, std::size_t code_size, std::uint64_t code_hash,
                           std::uint32_t *receiver_id, std::uint64_t *consumer_family_hash)
{
    if (code != nullptr && code_size >= 20) {
        const auto *b = static_cast<const std::uint8_t *>(code);
        if (std::memcmp(b, "DXBC", 4) == 0) {
            for (const auto &r : k_build131_pmetal_receivers) {
                if (r.code_size != code_size) continue;
                if (std::memcmp(b + 4, r.checksum.data(), r.checksum.size()) != 0) continue;
                if (receiver_id != nullptr) *receiver_id = r.receiver_id;
                if (consumer_family_hash != nullptr) *consumer_family_hash = 0x504D4554414C0001ull;
                return true;
            }
        }
    }
    return classify_receiver(code, code_size, code_hash, receiver_id, consumer_family_hash);
}

bool is_dedicated_pmetal_receiver(std::uint32_t id) noexcept
{
    return id == 20072u || id == 20073u || id == 20074u;
}

inline constexpr std::array<std::uint8_t,32> k_pmetal_raw_sha = {{
    0xec,0xe7,0x0f,0x36,0xbd,0x25,0x17,0xd2,0x8c,0x84,0x95,0xe2,0x76,0xce,0xa5,0x37,
    0xf8,0xb5,0x19,0xd6,0xbe,0xd9,0x81,0x78,0x8e,0x79,0xa4,0x09,0xff,0xbf,0x76,0x3b
}};

bool is_exact_pmetal(const material_record &m) noexcept
{
    return m.raw_sha == k_pmetal_raw_sha;
}

std::atomic<std::uint64_t> g_v33_dedicated_match{0};
std::atomic<std::uint64_t> g_v33_present_consumer_reject{0};
std::atomic<std::uint64_t> g_v33_raw_rgba_created{0};
std::atomic<std::uint64_t> g_v33_nonpmetal_present_failopen{0};

std::optional<reshade::api::resource_view> get_raw_rgba_cube_v33(
    std::uint16_t probe, std::uint8_t slot)
{
    if (g_device == nullptr || g_ptde_pack.empty() ||
        probe >= k_probe_count || slot >= k_ptde_slots)
        return std::nullopt;

    const std::uint32_t key = static_cast<std::uint32_t>(probe) * k_ptde_slots + slot;
    {
        std::lock_guard lock(g_mutex);
        const auto it = g_virtual_cubes.find(key);
        if (it != g_virtual_cubes.end()) return it->second.view;
    }

    const std::size_t cube_offset = static_cast<std::size_t>(key) * k_ptde_bytes_per_cube;
    const std::size_t face_bytes = static_cast<std::size_t>(k_ptde_size) * k_ptde_size * 4u;
    std::array<reshade::api::subresource_data,k_faces> sub{};
    for (std::uint32_t face = 0; face < k_faces; ++face) {
        sub[face].data = g_ptde_pack.data() + cube_offset + static_cast<std::size_t>(face) * face_bytes;
        sub[face].row_pitch = k_ptde_size * 4u;
        sub[face].slice_pitch = static_cast<std::uint32_t>(face_bytes);
    }

    const reshade::api::resource_desc desc(
        reshade::api::resource_type::texture_2d, k_ptde_size, k_ptde_size, k_faces, 1,
        reshade::api::format::r8g8b8a8_unorm, 1,
        reshade::api::memory_heap::default_, reshade::api::resource_usage::shader_resource,
        reshade::api::resource_flags::cube_compatible);

    virtual_cube cube{}; cube.probe_index = probe; cube.slot = slot;
    g_internal_resource_create = true;
    const bool resource_ok = g_device->create_resource(
        desc, sub.data(), reshade::api::resource_usage::shader_resource, &cube.resource);
    bool view_ok = false;
    if (resource_ok) {
        const reshade::api::resource_view_desc vd(
            reshade::api::resource_view_type::texture_cube,
            reshade::api::format::r8g8b8a8_unorm, 0, 1, 0, k_faces);
        view_ok = g_device->create_resource_view(
            cube.resource, reshade::api::resource_usage::shader_resource, vd, &cube.view);
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
    ++g_v33_raw_rgba_created;
    return cube.view;
}

struct replacement_plan_v33 {
    command_bindings original{};
    std::array<reshade::api::resource_view,2> replacement{};
    std::array<bool,2> replace{};
    bool use_exact_sampler = false;
};

std::optional<replacement_plan_v33> build_plan_v33(reshade::api::command_list *cmd)
{
    const auto pipeline = global_tracker().resolve_bound_pipeline(command_id(cmd));
    if (!pipeline.has_value() || !pipeline->confirmed) { ++g_receiver_reject; return std::nullopt; }
    ++g_receiver_match;

    const auto binding = active_binding();
    if (!binding.has_value() || binding->record == nullptr) {
        ++g_material_unknown; ++g_fail_open; return std::nullopt;
    }
    const material_record &m = *binding->record;
    const bool dedicated = is_dedicated_pmetal_receiver(pipeline->receiver_id);

    command_bindings current{};
    {
        std::lock_guard lock(g_mutex);
        const auto it = g_command_bindings.find(command_id(cmd));
        if (it == g_command_bindings.end()) { ++g_fail_open; return std::nullopt; }
        current = it->second;
    }

    replacement_plan_v33 plan{}; plan.original = current;
    if (m.state == material_envspec_state::present) {
        if (!is_exact_pmetal(m)) {
            ++g_v33_nonpmetal_present_failopen; ++g_material_unknown; ++g_fail_open;
            return std::nullopt;
        }
        if (!dedicated) {
            ++g_v33_present_consumer_reject; ++g_fail_open;
            return std::nullopt;
        }
        ++g_v33_dedicated_match;
        if (!current.srv[0].valid || !current.srv[1].valid) {
            ++g_resource_route_miss; ++g_fail_open; return std::nullopt;
        }
        const auto probe_a = probe_for_view(current.srv[0].view);
        const auto probe_b = probe_for_view(current.srv[1].view);
        if (!probe_a.has_value() || !probe_b.has_value()) {
            ++g_resource_route_miss; ++g_fail_open; return std::nullopt;
        }
        const auto raw_a = get_raw_rgba_cube_v33(*probe_a, m.slot);
        const auto raw_b = get_raw_rgba_cube_v33(*probe_b, m.slot);
        if (!raw_a.has_value() || !raw_b.has_value()) {
            ++g_resource_route_miss; ++g_fail_open; return std::nullopt;
        }
        plan.replacement[0] = *raw_a;
        plan.replacement[1] = *raw_b;
        plan.replace[0] = true;
        plan.replace[1] = true;
        plan.use_exact_sampler = true;
        ++g_resource_route_hit; ++g_resource_route_hit;
        ++g_material_present;
        return plan;
    }

    if (m.state == material_envspec_state::explicit_none && binding->explicit_none_safe) {
        if (dedicated) { ++g_fail_open; return std::nullopt; }
        if (g_black_view.handle == 0 && !create_black_cube()) { ++g_fail_open; return std::nullopt; }
        bool any = false;
        for (int i = 0; i < 2; ++i) {
            if (!current.srv[i].valid) continue;
            plan.replacement[i] = g_black_view;
            plan.replace[i] = true;
            any = true;
        }
        if (!any) { ++g_resource_route_miss; ++g_fail_open; return std::nullopt; }
        ++g_material_explicit_none;
        return plan;
    }

    ++g_material_unknown; ++g_fail_open;
    return std::nullopt;
}

bool apply_plan_v33(reshade::api::command_list *cmd, const replacement_plan_v33 &plan)
{
    bool any = false;
    for (int i = 0; i < 2; ++i) {
        if (!plan.replace[i]) continue;
        const std::uint32_t slot = i == 0 ? 12u : 14u;
        if (push_srv_binding(cmd, slot, plan.original.srv[i], plan.replacement[i], plan.use_exact_sampler)) {
            any = true;
            if (slot == 12) ++g_t12_rebind; else ++g_t14_rebind;
        }
        if (plan.use_exact_sampler && plan.original.sampler[i].valid && g_ptde_sampler.handle != 0) {
            if (push_sampler_binding(cmd, slot, plan.original.sampler[i], g_ptde_sampler)) ++g_sampler_rebind;
        }
    }
    return any;
}

bool restore_plan_v33(reshade::api::command_list *cmd, const replacement_plan_v33 &plan)
{
    bool ok = true;
    for (int i = 0; i < 2; ++i) {
        if (!plan.replace[i]) continue;
        const std::uint32_t slot = i == 0 ? 12u : 14u;
        ok = push_srv_binding(cmd, slot, plan.original.srv[i], plan.original.srv[i].view, false) && ok;
        if (plan.original.sampler[i].valid)
            ok = push_sampler_binding(cmd, slot, plan.original.sampler[i], plan.original.sampler[i].sampler) && ok;
    }
    if (ok) ++g_restore_pass; else ++g_restore_fail;
    return ok;
}

bool on_draw_v33(reshade::api::command_list *cmd, std::uint32_t vertex_count,
                 std::uint32_t instance_count, std::uint32_t first_vertex,
                 std::uint32_t first_instance)
{
    if (g_internal_replay) return false;
    const auto plan = build_plan_v33(cmd);
    g_active_material = 0;
    if (!plan.has_value() || !apply_plan_v33(cmd, *plan)) return false;
    g_internal_replay = true;
    cmd->draw(vertex_count, instance_count, first_vertex, first_instance);
    g_internal_replay = false;
    restore_plan_v33(cmd, *plan);
    ++g_replay_pass;
    return true;
}

bool on_draw_indexed_v33(reshade::api::command_list *cmd, std::uint32_t index_count,
                         std::uint32_t instance_count, std::uint32_t first_index,
                         std::int32_t vertex_offset, std::uint32_t first_instance)
{
    if (g_internal_replay) return false;
    const auto plan = build_plan_v33(cmd);
    g_active_material = 0;
    if (!plan.has_value() || !apply_plan_v33(cmd, *plan)) return false;
    g_internal_replay = true;
    cmd->draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);
    g_internal_replay = false;
    restore_plan_v33(cmd, *plan);
    ++g_replay_pass;
    return true;
}

void log_snapshot_v33(const runtime_snapshot &snapshot)
{
    const std::uint64_t p = ++g_present_count;
    if (p != 1 && (p % 300) != 0) return;
    char line[1900] = {};
    std::snprintf(line, sizeof(line),
        "[DSRRL FULL ENVSPEC V3.3] P=%llu EXE=%llu/%llu HOOK=%llu/%llu "
        "PARSER=%llu mapped=%llu unmapped=%llu SELECTOR=%llu resolved=%llu "
        "native=%llu/%llu miss=%llu raw=%llu recv=%llu reject=%llu "
        "dedicated=%llu present_reject=%llu nonpmetal_present=%llu "
        "mat_present=%llu explicit_none=%llu unknown=%llu route=%llu miss=%llu "
        "t12=%llu t14=%llu sampler=%llu replay=%llu restore=%llu/%llu failopen=%llu "
        "LIVE res=%llu view=%llu pipe=%llu cmd=%llu",
        static_cast<unsigned long long>(p),
        static_cast<unsigned long long>(g_exe_guard_pass.load()),
        static_cast<unsigned long long>(g_exe_guard_fail.load()),
        static_cast<unsigned long long>(g_hook_install_pass.load()),
        static_cast<unsigned long long>(g_hook_install_fail.load()),
        static_cast<unsigned long long>(g_parser_seen.load()),
        static_cast<unsigned long long>(g_material_mapped.load()),
        static_cast<unsigned long long>(g_material_unmapped.load()),
        static_cast<unsigned long long>(g_selector_seen.load()),
        static_cast<unsigned long long>(g_selector_resolved.load()),
        static_cast<unsigned long long>(g_native_matches.load()),
        static_cast<unsigned long long>(g_native_candidates.load()),
        static_cast<unsigned long long>(g_native_hash_miss.load()),
        static_cast<unsigned long long>(g_v33_raw_rgba_created.load()),
        static_cast<unsigned long long>(g_receiver_match.load()),
        static_cast<unsigned long long>(g_receiver_reject.load()),
        static_cast<unsigned long long>(g_v33_dedicated_match.load()),
        static_cast<unsigned long long>(g_v33_present_consumer_reject.load()),
        static_cast<unsigned long long>(g_v33_nonpmetal_present_failopen.load()),
        static_cast<unsigned long long>(g_material_present.load()),
        static_cast<unsigned long long>(g_material_explicit_none.load()),
        static_cast<unsigned long long>(g_material_unknown.load()),
        static_cast<unsigned long long>(g_resource_route_hit.load()),
        static_cast<unsigned long long>(g_resource_route_miss.load()),
        static_cast<unsigned long long>(g_t12_rebind.load()),
        static_cast<unsigned long long>(g_t14_rebind.load()),
        static_cast<unsigned long long>(g_sampler_rebind.load()),
        static_cast<unsigned long long>(g_replay_pass.load()),
        static_cast<unsigned long long>(g_restore_pass.load()),
        static_cast<unsigned long long>(g_restore_fail.load()),
        static_cast<unsigned long long>(g_fail_open.load()),
        static_cast<unsigned long long>(snapshot.live_resources),
        static_cast<unsigned long long>(snapshot.live_views),
        static_cast<unsigned long long>(snapshot.live_pipelines),
        static_cast<unsigned long long>(snapshot.live_commands));
    reshade::log::message(reshade::log::level::info, line);
}

void register_v33_events()
{
    reshade::register_event<reshade::addon_event::init_device>(on_init_device);
    reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
    reshade::register_event<reshade::addon_event::init_command_list>(on_init_command_list);
    reshade::register_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
    reshade::register_event<reshade::addon_event::init_resource>(on_init_resource);
    reshade::register_event<reshade::addon_event::destroy_resource>(on_destroy_resource);
    reshade::register_event<reshade::addon_event::init_resource_view>(on_init_resource_view);
    reshade::register_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);
    reshade::register_event<reshade::addon_event::push_descriptors>(on_push_descriptors);
    reshade::register_event<reshade::addon_event::draw>(on_draw_v33);
    reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed_v33);
}

void unregister_v33_events()
{
    reshade::unregister_event<reshade::addon_event::draw_indexed>(on_draw_indexed_v33);
    reshade::unregister_event<reshade::addon_event::draw>(on_draw_v33);
    reshade::unregister_event<reshade::addon_event::push_descriptors>(on_push_descriptors);
    reshade::unregister_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);
    reshade::unregister_event<reshade::addon_event::init_resource_view>(on_init_resource_view);
    reshade::unregister_event<reshade::addon_event::destroy_resource>(on_destroy_resource);
    reshade::unregister_event<reshade::addon_event::init_resource>(on_init_resource);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
    reshade::unregister_event<reshade::addon_event::init_command_list>(on_init_command_list);
    reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
    reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
}

} // namespace
} // namespace dsrrl::runtime_v2::full_envspec_v3

extern "C" __declspec(dllexport) const char *NAME =
    "DSRRL PTDE Full EnvSpec Carrier V3.3 Raw RGBA Dedicated P_Metal";
extern "C" __declspec(dllexport) const char *DESCRIPTION =
    "Build131 coexistence; raw PTDE RGBA filtered before shader decode; exact P_Metal dedicated-consumer gate; explicit-none safe subset; fail-open elsewhere.";

extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon_module, HMODULE reshade_module)
{
    if (!reshade::register_addon(addon_module, reshade_module)) return false;
    using namespace dsrrl::runtime_v2;
    using namespace dsrrl::runtime_v2::full_envspec_v3;
    set_receiver_classifier(classify_receiver_v33);
    set_snapshot_sink(log_snapshot_v33);
    register_reshade_events();
    register_v33_events();
    if (!load_pack(addon_module)) {
        reshade::log::message(reshade::log::level::error,
            "[DSRRL FULL ENVSPEC V3.3] canonical PTDE pack load/SHA failed; PRESENT stays fail-open");
    }
    if (!load_router_sidecars(addon_module)) {
        reshade::log::message(reshade::log::level::error,
            "[DSRRL FULL ENVSPEC V3.3] material/probe sidecar load/SHA failed; bridge stays fail-open");
    }
    if (!verify_and_install_compat_hooks_v33()) {
        reshade::log::message(reshade::log::level::error,
            "[DSRRL FULL ENVSPEC V3.3] retail callsite guard failed; bridge stays fail-open");
    } else {
        reshade::log::message(reshade::log::level::info,
            "[DSRRL FULL ENVSPEC V3.3] build131 callsites PASS; raw RGBA + dedicated P_Metal consumer gate armed");
    }
    return true;
}

extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon_module, HMODULE reshade_module)
{
    using namespace dsrrl::runtime_v2;
    using namespace dsrrl::runtime_v2::full_envspec_v3;
    unpatch_callsites();
    unregister_v33_events();
    unregister_reshade_events();
    set_receiver_classifier(nullptr);
    set_snapshot_sink(nullptr);
    {
        std::lock_guard lock(g_mutex);
        g_materials.clear();
        g_native_resources.clear();
        g_source_resource_by_view.clear();
        g_command_bindings.clear();
    }
    g_ptde_pack.clear();
    g_material_registry.clear();
    g_native_probe_registry.clear();
    reshade::unregister_addon(addon_module, reshade_module);
}
