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

constexpr std::uintptr_t k_parser_thunk_rva = 0x29DFC0;
constexpr std::array<std::uintptr_t, 4> k_compat_call_rvas = {
    0x291D9C, // parser thunk call
    0x20E014, // selector call 1
    0x20EB7A, // selector call 2
    0x20FB99  // selector call 3
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

void remember_material_binding(void *material, const void *raw, std::uint32_t size, const wchar_t *semantic_key)
{
    material_binding binding{};
    if (material != nullptr && raw != nullptr && size != 0 && size <= (1u << 20)) {
        std::array<std::uint8_t,32> sha{};
        const std::uint64_t name_hash = fnv1a_utf16_basename_lower_ascii(semantic_key);
        if (sha256_bytes(raw, size, sha)) {
            binding.record = find_material(name_hash, sha);
            if (binding.record != nullptr && binding.record->state == material_envspec_state::explicit_none)
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

void __fastcall parser_callsite_hook(void *owner, const void *raw, std::uint32_t size)
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
    remember_material_binding(material, raw, size, semantic_key);
}

std::uintptr_t __fastcall selector_callsite_hook(
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

bool verify_and_install_compat_hooks()
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
                        reinterpret_cast<const void *>(&parser_callsite_hook), k_compat_expected_calls[0]) ||
        !patch_callsite(1, base + k_compat_call_rvas[1],
                        reinterpret_cast<const void *>(&selector_callsite_hook), k_compat_expected_calls[1]) ||
        !patch_callsite(2, base + k_compat_call_rvas[2],
                        reinterpret_cast<const void *>(&selector_callsite_hook), k_compat_expected_calls[2]) ||
        !patch_callsite(3, base + k_compat_call_rvas[3],
                        reinterpret_cast<const void *>(&selector_callsite_hook), k_compat_expected_calls[3])) {
        unpatch_callsites();
        ++g_hook_install_fail;
        return false;
    }

    ++g_hook_install_pass;
    return true;
}

} // namespace
} // namespace dsrrl::runtime_v2::full_envspec_v3

extern "C" __declspec(dllexport) const char *NAME = "DSRRL PTDE Full EnvSpec Carrier V3.2 Build131 Compat";
extern "C" __declspec(dllexport) const char *DESCRIPTION =
    "Build131-compatible exact material/probe PTDE EnvSpec carrier; observes retail callsites without replacing Material Response entry hooks.";

extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon_module, HMODULE reshade_module)
{
    if (!reshade::register_addon(addon_module, reshade_module)) return false;
    using namespace dsrrl::runtime_v2;
    using namespace dsrrl::runtime_v2::full_envspec_v3;
    set_receiver_classifier(classify_receiver);
    set_snapshot_sink(log_snapshot);
    register_reshade_events();
    register_v3_events();
    if (!load_pack(addon_module)) {
        reshade::log::message(reshade::log::level::error,
            "[DSRRL FULL ENVSPEC CARRIER V3.2] canonical PTDE pack load/SHA failed; PRESENT route stays fail-open");
    }
    if (!load_router_sidecars(addon_module)) {
        reshade::log::message(reshade::log::level::error,
            "[DSRRL FULL ENVSPEC CARRIER V3.2] material/probe sidecar load/SHA failed; bridge stays fail-open");
    }
    if (!verify_and_install_compat_hooks()) {
        reshade::log::message(reshade::log::level::error,
            "[DSRRL FULL ENVSPEC CARRIER V3.2] retail callsite guard failed; EnvSpec bridge stays fail-open");
    } else {
        reshade::log::message(reshade::log::level::info,
            "[DSRRL FULL ENVSPEC CARRIER V3.2] build131 coexistence callsites PASS; exact material routing armed");
    }
    return true;
}

extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon_module, HMODULE reshade_module)
{
    using namespace dsrrl::runtime_v2;
    using namespace dsrrl::runtime_v2::full_envspec_v3;
    unpatch_callsites();
    unregister_v3_events();
    unregister_reshade_events();
    set_receiver_classifier(nullptr);
    set_snapshot_sink(nullptr);
    {
        std::lock_guard lock(g_mutex);
        g_materials.clear(); g_native_resources.clear(); g_source_resource_by_view.clear();
        g_command_bindings.clear();
    }
    g_ptde_pack.clear(); g_material_registry.clear(); g_native_probe_registry.clear();
    reshade::unregister_addon(addon_module, reshade_module);
}
