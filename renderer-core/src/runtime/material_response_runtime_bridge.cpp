#include "dsrrl/runtime/material_response_runtime_bridge.hpp"

#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include "dsrrl/runtime/material_response_shader_materializer.hpp"
#include "dsrrl/runtime/ptde_material_donor_registry.hpp"

#include <Windows.h>
#include <d3d11_1.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace reshade::api;

extern "C" void selector_hook_entry();
extern "C" {
void *g_selector_trampoline = nullptr;
}

namespace {

constexpr std::string_view k_exe_sha256 =
    "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b";

constexpr std::uintptr_t k_rva_selector = 0x22BA20u;
constexpr std::uintptr_t k_rva_mtd_parse_wrapper = 0x295ED0u;

constexpr std::uintptr_t k_ret_sel_1 = 0x20E019u;
constexpr std::uintptr_t k_ret_sel_2 = 0x20EB7Fu;
constexpr std::uintptr_t k_ret_sel_3 = 0x20FB9Eu;

constexpr std::array<std::uint8_t, 15> k_bytes_selector = {
    0x40,0x53,0x48,0x83,0xEC,0x30,0x49,0x63,
    0xC0,0x45,0x8B,0xD1,0x48,0x8B,0xDA
};

constexpr std::array<std::uint8_t, 15> k_bytes_mtd_parse_wrapper = {
    0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0xC7,
    0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF
};

struct native_hook {
    void *target = nullptr;
    void *trampoline = nullptr;
    void *detour = nullptr;
    std::size_t stolen = 0;
    std::array<std::uint8_t, 32> original{};
    bool patched = false;
};

native_hook g_selector_hook{};
native_hook g_mtd_hook{};

using mtd_parse_fn =
    void (__fastcall *)(void *, const void *, std::uint32_t, void *);

mtd_parse_fn g_mtd_parse_original = nullptr;

std::uintptr_t g_exe_base = 0;

std::atomic<std::uint64_t> g_mtd_seen{0};
std::atomic<std::uint64_t> g_donor_registered{0};
std::atomic<std::uint64_t> g_donor_unmapped{0};
std::atomic<std::uint64_t> g_selector_seen{0};
std::atomic<std::uint64_t> g_selector_donor{0};
std::atomic<std::uint64_t> g_b12_created{0};
std::atomic<std::uint64_t> g_b12_cache_hits{0};

std::mutex g_material_mutex;
std::unordered_map<void *, std::uint16_t> g_material_donor;

thread_local int g_draw_donor = -1;
thread_local bool g_internal_shader_create = false;
thread_local bool g_replaying_draw = false;

struct alternate_pair {
    ID3D11PixelShader *diffuse = nullptr;
    ID3D11PixelShader *full = nullptr;
};

struct device_state {
    ID3D11Device *device = nullptr;
    std::array<alternate_pair, 24> alternates{};
    std::unordered_map<std::uint16_t, ID3D11Buffer *> b12;
};

std::mutex g_device_mutex;
device_state g_device;

std::filesystem::path process_path()
{
    std::wstring buffer(32768, L'\0');
    const DWORD size = GetModuleFileNameW(
        nullptr,
        buffer.data(),
        static_cast<DWORD>(buffer.size()));

    if (size == 0 || size >= buffer.size())
        return {};

    buffer.resize(size);
    return std::filesystem::path(buffer);
}

std::string hex_digest(
    const dsrrl::operators::legacy_plan::hashing::sha256_digest &digest)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.resize(64u);

    for (std::size_t i = 0; i < digest.size(); ++i) {
        out[2u * i] = digits[digest[i] >> 4u];
        out[2u * i + 1u] = digits[digest[i] & 0x0Fu];
    }

    return out;
}

bool verify_exe_provenance()
{
    try {
        const auto path = process_path();
        if (path.empty())
            return false;

        std::ifstream input(path, std::ios::binary);
        if (!input)
            return false;

        input.seekg(0, std::ios::end);
        const auto end = input.tellg();
        if (end <= 0)
            return false;

        const auto size = static_cast<std::size_t>(end);
        std::vector<std::uint8_t> bytes(size);

        input.seekg(0, std::ios::beg);
        input.read(
            reinterpret_cast<char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));

        if (!input)
            return false;

        const auto digest =
            dsrrl::operators::legacy_plan::hashing::sha256(
                bytes.data(),
                bytes.size());

        return hex_digest(digest) == k_exe_sha256;
    }
    catch (...) {
        return false;
    }
}

template <class T>
bool safe_read(const void *source, T &out) noexcept
{
#if defined(_MSC_VER)
    __try {
        std::memcpy(&out, source, sizeof(T));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    if (source == nullptr)
        return false;
    std::memcpy(&out, source, sizeof(T));
    return true;
#endif
}

bool safe_copy(
    void *destination,
    const void *source,
    std::size_t size) noexcept
{
#if defined(_MSC_VER)
    __try {
        std::memcpy(destination, source, size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    if (destination == nullptr || source == nullptr)
        return false;
    std::memcpy(destination, source, size);
    return true;
#endif
}

bool write_bytes(
    void *destination,
    const void *source,
    std::size_t size) noexcept
{
    DWORD old_protection = 0;
    if (!VirtualProtect(
            destination,
            size,
            PAGE_EXECUTE_READWRITE,
            &old_protection))
        return false;

    std::memcpy(destination, source, size);

    const bool flushed =
        FlushInstructionCache(
            GetCurrentProcess(),
            destination,
            size) != FALSE;

    DWORD ignored = 0;
    const bool restored =
        VirtualProtect(
            destination,
            size,
            old_protection,
            &ignored) != FALSE;

    return flushed && restored;
}

template <std::size_t N>
bool prepare_hook(
    native_hook &hook,
    std::uintptr_t rva,
    const std::array<std::uint8_t, N> &expected,
    void *detour) noexcept
{
    static_assert(N >= 14u && N <= 32u);

    auto *target =
        reinterpret_cast<std::uint8_t *>(g_exe_base + rva);

    std::array<std::uint8_t, N> current{};
    if (!safe_copy(
            current.data(),
            target,
            current.size()) ||
        current != expected)
        return false;

    auto *trampoline =
        static_cast<std::uint8_t *>(
            VirtualAlloc(
                nullptr,
                N + 14u,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_EXECUTE_READWRITE));

    if (trampoline == nullptr)
        return false;

    std::memcpy(trampoline, target, N);

    auto *tail = trampoline + N;
    tail[0] = 0xFFu;
    tail[1] = 0x25u;
    std::uint32_t zero = 0;
    std::memcpy(tail + 2u, &zero, sizeof(zero));

    const auto resume =
        reinterpret_cast<std::uint64_t>(target + N);
    std::memcpy(tail + 6u, &resume, sizeof(resume));

    if (FlushInstructionCache(
            GetCurrentProcess(),
            trampoline,
            N + 14u) == FALSE) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }

    hook.target = target;
    hook.trampoline = trampoline;
    hook.detour = detour;
    hook.stolen = N;
    std::copy(
        current.begin(),
        current.end(),
        hook.original.begin());

    return true;
}

bool patch_hook(native_hook &hook) noexcept
{
    if (hook.target == nullptr ||
        hook.trampoline == nullptr ||
        hook.detour == nullptr ||
        hook.stolen < 14u ||
        hook.stolen > hook.original.size())
        return false;

    std::array<std::uint8_t, 32> patch{};
    patch.fill(0x90u);
    patch[0] = 0xFFu;
    patch[1] = 0x25u;

    std::uint32_t zero = 0;
    std::memcpy(patch.data() + 2u, &zero, sizeof(zero));

    const auto detour =
        reinterpret_cast<std::uint64_t>(hook.detour);
    std::memcpy(patch.data() + 6u, &detour, sizeof(detour));

    if (!write_bytes(
            hook.target,
            patch.data(),
            hook.stolen))
        return false;

    hook.patched = true;
    return true;
}

void restore_hook(native_hook &hook) noexcept
{
    if (hook.patched) {
        static_cast<void>(
            write_bytes(
                hook.target,
                hook.original.data(),
                hook.stolen));
        hook.patched = false;
    }

    if (hook.trampoline != nullptr) {
        VirtualFree(
            hook.trampoline,
            0,
            MEM_RELEASE);
        hook.trampoline = nullptr;
    }

    hook.target = nullptr;
    hook.detour = nullptr;
    hook.stolen = 0;
}

int lookup_donor(void *material) noexcept
{
    if (material == nullptr)
        return -1;

    std::lock_guard<std::mutex> lock(g_material_mutex);
    const auto found = g_material_donor.find(material);
    if (found == g_material_donor.end())
        return -1;

    return static_cast<int>(found->second);
}

void register_material(
    void *material,
    const void *raw,
    std::uint32_t length) noexcept
{
    if (material == nullptr ||
        raw == nullptr ||
        length == 0u)
        return;

    ++g_mtd_seen;

    try {
        const auto digest =
            dsrrl::operators::legacy_plan::hashing::sha256(
                static_cast<const std::uint8_t *>(raw),
                length);

        const auto hash = hex_digest(digest);
        const int donor =
            dsrrl::materialdonor::find_sha256(hash);

        std::lock_guard<std::mutex> lock(g_material_mutex);
        g_material_donor.erase(material);

        if (donor >= 0) {
            g_material_donor[material] =
                static_cast<std::uint16_t>(donor);
            ++g_donor_registered;
        } else {
            ++g_donor_unmapped;
        }
    }
    catch (...) {
        ++g_donor_unmapped;
    }
}

void __fastcall hook_mtd_parse(
    void *material,
    const void *raw,
    std::uint32_t length,
    void *arg4) noexcept
{
    register_material(material, raw, length);

    if (g_mtd_parse_original != nullptr)
        g_mtd_parse_original(
            material,
            raw,
            length,
            arg4);
}

void *resolve_selector_material(
    void *container,
    std::int32_t index) noexcept
{
    if (container == nullptr ||
        index < 0 ||
        index > 0x100000)
        return nullptr;

    void *array = nullptr;
    if (!safe_read(
            static_cast<const std::uint8_t *>(container) + 0x10u,
            array) ||
        array == nullptr)
        return nullptr;

    void *material = nullptr;
    const auto *entry =
        static_cast<const std::uint8_t *>(array) +
        static_cast<std::size_t>(index) * 24u;

    if (!safe_read(entry, material))
        return nullptr;

    return material;
}

extern "C" void selector_observer_impl(
    void *container,
    void *,
    void *return_address,
    void *,
    void *,
    std::int32_t index) noexcept
{
    ++g_selector_seen;

    const auto rva =
        reinterpret_cast<std::uintptr_t>(
            return_address) -
        g_exe_base;

    if (rva != k_ret_sel_1 &&
        rva != k_ret_sel_2 &&
        rva != k_ret_sel_3) {
        g_draw_donor = -1;
        return;
    }

    g_draw_donor =
        lookup_donor(
            resolve_selector_material(
                container,
                index));

    if (g_draw_donor >= 0)
        ++g_selector_donor;
}

extern "C" void selector_observer(
    void *container,
    void *owner,
    void *return_address,
    void *r14,
    void *r15,
    std::int32_t index) noexcept
{
    try {
        selector_observer_impl(
            container,
            owner,
            return_address,
            r14,
            r15,
            index);
    }
    catch (...) {
        g_draw_donor = -1;
    }
}

bool install_hooks() noexcept
{
    if (!prepare_hook(
            g_selector_hook,
            k_rva_selector,
            k_bytes_selector,
            reinterpret_cast<void *>(
                &selector_hook_entry)))
        return false;

    g_selector_trampoline =
        g_selector_hook.trampoline;

    if (!prepare_hook(
            g_mtd_hook,
            k_rva_mtd_parse_wrapper,
            k_bytes_mtd_parse_wrapper,
            reinterpret_cast<void *>(
                &hook_mtd_parse))) {
        restore_hook(g_selector_hook);
        g_selector_trampoline = nullptr;
        return false;
    }

    g_mtd_parse_original =
        reinterpret_cast<mtd_parse_fn>(
            g_mtd_hook.trampoline);

    if (!patch_hook(g_mtd_hook) ||
        !patch_hook(g_selector_hook)) {
        restore_hook(g_selector_hook);
        restore_hook(g_mtd_hook);
        g_selector_trampoline = nullptr;
        g_mtd_parse_original = nullptr;
        return false;
    }

    return true;
}

void uninstall_hooks() noexcept
{
    restore_hook(g_selector_hook);
    restore_hook(g_mtd_hook);
    g_selector_trampoline = nullptr;
    g_mtd_parse_original = nullptr;
}

const shader_desc *find_pixel_shader(
    std::uint32_t subobject_count,
    const pipeline_subobject *subobjects) noexcept
{
    if (subobjects == nullptr)
        return nullptr;

    for (std::uint32_t i = 0;
         i < subobject_count;
         ++i) {
        if (subobjects[i].type ==
                pipeline_subobject_type::pixel_shader &&
            subobjects[i].count == 1u &&
            subobjects[i].data != nullptr)
            return static_cast<const shader_desc *>(
                subobjects[i].data);
    }

    return nullptr;
}

void release_device_state() noexcept
{
    std::lock_guard<std::mutex> lock(g_device_mutex);

    for (auto &pair : g_device.alternates) {
        if (pair.diffuse != nullptr) {
            pair.diffuse->Release();
            pair.diffuse = nullptr;
        }

        if (pair.full != nullptr) {
            pair.full->Release();
            pair.full = nullptr;
        }
    }

    for (auto &entry : g_device.b12)
        if (entry.second != nullptr)
            entry.second->Release();

    g_device.b12.clear();

    if (g_device.device != nullptr) {
        g_device.device->Release();
        g_device.device = nullptr;
    }
}

bool prepare_alternate_pair(
    device *reshade_device,
    const dsrrl::runtime::material_response_generated::host_recipe &recipe,
    const std::uint8_t *source,
    std::size_t source_size) noexcept
{
    if (reshade_device == nullptr ||
        source == nullptr ||
        recipe.stable_index >= g_device.alternates.size())
        return false;

    auto *native_device =
        reinterpret_cast<ID3D11Device *>(
            reshade_device->get_native());

    if (native_device == nullptr)
        return false;

    {
        std::lock_guard<std::mutex> lock(g_device_mutex);

        if (g_device.device != native_device)
            return false;

        const auto &pair =
            g_device.alternates[recipe.stable_index];

        if (pair.diffuse != nullptr &&
            pair.full != nullptr)
            return true;
    }

    std::vector<std::uint8_t> diffuse;
    std::vector<std::uint8_t> full;

    const auto diffuse_outcome =
        dsrrl::runtime::materialize_material_response_shader(
            source,
            source_size,
            false,
            diffuse);

    const auto full_outcome =
        dsrrl::runtime::materialize_material_response_shader(
            source,
            source_size,
            true,
            full);

    if (diffuse_outcome.result !=
            dsrrl::runtime::material_response_materialize_result::applied ||
        full_outcome.result !=
            dsrrl::runtime::material_response_materialize_result::applied)
        return false;

    ID3D11PixelShader *diffuse_shader = nullptr;
    ID3D11PixelShader *full_shader = nullptr;

    const bool previous_internal =
        g_internal_shader_create;
    g_internal_shader_create = true;

    const HRESULT diffuse_hr =
        native_device->CreatePixelShader(
            diffuse.data(),
            diffuse.size(),
            nullptr,
            &diffuse_shader);

    const HRESULT full_hr =
        native_device->CreatePixelShader(
            full.data(),
            full.size(),
            nullptr,
            &full_shader);

    g_internal_shader_create =
        previous_internal;

    if (FAILED(diffuse_hr) ||
        diffuse_shader == nullptr ||
        FAILED(full_hr) ||
        full_shader == nullptr) {
        if (diffuse_shader != nullptr)
            diffuse_shader->Release();
        if (full_shader != nullptr)
            full_shader->Release();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(g_device_mutex);

        if (g_device.device != native_device) {
            diffuse_shader->Release();
            full_shader->Release();
            return false;
        }

        auto &pair =
            g_device.alternates[recipe.stable_index];

        if (pair.diffuse == nullptr &&
            pair.full == nullptr) {
            pair.diffuse = diffuse_shader;
            pair.full = full_shader;
            return true;
        }

        diffuse_shader->Release();
        full_shader->Release();

        return pair.diffuse != nullptr &&
               pair.full != nullptr;
    }
}

ID3D11Buffer *realize_b12(
    ID3D11Device *device,
    int donor_index) noexcept
{
    if (device == nullptr ||
        donor_index < 0 ||
        static_cast<std::size_t>(donor_index) >=
            dsrrl::materialdonor::k_donors.size())
        return nullptr;

    std::lock_guard<std::mutex> lock(g_device_mutex);

    if (g_device.device != device)
        return nullptr;

    const auto key =
        static_cast<std::uint16_t>(donor_index);

    const auto existing =
        g_device.b12.find(key);

    if (existing != g_device.b12.end() &&
        existing->second != nullptr) {
        existing->second->AddRef();
        ++g_b12_cache_hits;
        return existing->second;
    }

    const auto &donor =
        dsrrl::materialdonor::k_donors[
            static_cast<std::size_t>(donor_index)];

    struct float4 {
        float x;
        float y;
        float z;
        float w;
    };

    const std::array<float4, 4> payload{{
        {
            donor.c101_f0q[0],
            donor.c101_f0q[1],
            donor.c101_f0q[2],
            donor.has_c101 ? 1.0f : 0.0f
        },
        {
            donor.c100[0],
            donor.c100[1],
            donor.c100[2],
            1.0f
        },
        {0.0f,0.0f,0.0f,0.0f},
        {0.0f,0.0f,0.0f,0.0f}
    }};

    D3D11_BUFFER_DESC description{};
    description.ByteWidth = 64u;
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags =
        D3D11_BIND_CONSTANT_BUFFER;

    D3D11_SUBRESOURCE_DATA initial{};
    initial.pSysMem = payload.data();

    ID3D11Buffer *buffer = nullptr;

    if (FAILED(
            device->CreateBuffer(
                &description,
                &initial,
                &buffer)) ||
        buffer == nullptr)
        return nullptr;

    g_device.b12.emplace(key, buffer);
    buffer->AddRef();
    ++g_b12_created;
    return buffer;
}

struct constant_buffer_capture {
    ID3D11Buffer *base = nullptr;
    ID3D11Buffer *window = nullptr;
    UINT first = 0;
    UINT count = 0;
    bool has_window = false;
    bool coherent = true;
};

constant_buffer_capture capture_b12(
    ID3D11DeviceContext *context,
    ID3D11DeviceContext1 *context1) noexcept
{
    constant_buffer_capture capture;

    context->PSGetConstantBuffers(
        12u,
        1u,
        &capture.base);

    if (context1 != nullptr) {
        context1->PSGetConstantBuffers1(
            12u,
            1u,
            &capture.window,
            &capture.first,
            &capture.count);

        capture.coherent =
            capture.base == capture.window;

        capture.has_window =
            capture.window != nullptr &&
            capture.count >= 16u;
    }

    return capture;
}

void release_capture(
    constant_buffer_capture &capture) noexcept
{
    if (capture.base != nullptr)
        capture.base->Release();

    if (capture.window != nullptr)
        capture.window->Release();

    capture = {};
}

void restore_b12(
    ID3D11DeviceContext *context,
    ID3D11DeviceContext1 *context1,
    const constant_buffer_capture &capture) noexcept
{
    if (context1 != nullptr &&
        capture.has_window) {
        ID3D11Buffer *buffer =
            capture.window;
        UINT first = capture.first;
        UINT count = capture.count;

        context1->PSSetConstantBuffers1(
            12u,
            1u,
            &buffer,
            &first,
            &count);
    } else {
        ID3D11Buffer *buffer =
            capture.base;

        context->PSSetConstantBuffers(
            12u,
            1u,
            &buffer);
    }
}

} // namespace

namespace dsrrl::runtime {

material_response_runtime_bridge::
material_response_runtime_bridge(
    core::feature_registry &features) noexcept
    : features_(features)
{
}

bool material_response_runtime_bridge::start() noexcept
{
    if (hooks_active_.load())
        return true;

    if (!verify_exe_provenance())
        return false;

    g_exe_base =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(nullptr));

    if (g_exe_base == 0u ||
        !install_hooks()) {
        g_exe_base = 0u;
        return false;
    }

    hooks_active_.store(true);
    return true;
}

void material_response_runtime_bridge::stop() noexcept
{
    hooks_active_.store(false);

    uninstall_hooks();
    g_exe_base = 0u;
    g_draw_donor = -1;

    {
        std::lock_guard<std::mutex> lock(g_material_mutex);
        g_material_donor.clear();
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        pipelines_.clear();
        bound_by_command_.clear();
    }

    release_device_state();
}

void material_response_runtime_bridge::on_init_device(
    device *reshade_device) noexcept
{
    if (reshade_device == nullptr ||
        reshade_device->get_api() !=
            device_api::d3d11)
        return;

    auto *native_device =
        reinterpret_cast<ID3D11Device *>(
            reshade_device->get_native());

    if (native_device == nullptr) {
        quarantined_.store(true);
        return;
    }

    release_device_state();

    std::lock_guard<std::mutex> lock(g_device_mutex);
    g_device.device = native_device;
    native_device->AddRef();
}

void material_response_runtime_bridge::on_destroy_device(
    device *reshade_device) noexcept
{
    if (reshade_device == nullptr ||
        reshade_device->get_api() !=
            device_api::d3d11)
        return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        pipelines_.clear();
        bound_by_command_.clear();
    }

    release_device_state();
}

void material_response_runtime_bridge::on_init_pipeline(
    device *reshade_device,
    pipeline_layout,
    std::uint32_t subobject_count,
    const pipeline_subobject *subobjects,
    pipeline pipeline_object) noexcept
{
    if (g_internal_shader_create ||
        !hooks_active_.load() ||
        quarantined_.load() ||
        reshade_device == nullptr ||
        reshade_device->get_api() !=
            device_api::d3d11 ||
        pipeline_object.handle == 0u)
        return;

    const auto *pixel_shader =
        find_pixel_shader(
            subobject_count,
            subobjects);

    if (pixel_shader == nullptr ||
        pixel_shader->code == nullptr ||
        pixel_shader->code_size == 0u)
        return;

    const auto *recipe =
        material_response_materializer_detail::
            find_recipe(
                static_cast<const std::uint8_t *>(
                    pixel_shader->code),
                pixel_shader->code_size);

    if (recipe == nullptr)
        return;

    ++exact_pipeline_hits_;

    if (!prepare_alternate_pair(
            reshade_device,
            *recipe,
            static_cast<const std::uint8_t *>(
                pixel_shader->code),
            pixel_shader->code_size)) {
        ++fail_open_;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        pipelines_[pipeline_object.handle] = {
            recipe->stable_index,
            recipe->receiver_id
        };
    }

    ++alternate_pairs_ready_;
}

void material_response_runtime_bridge::on_destroy_pipeline(
    device *,
    pipeline pipeline_object) noexcept
{
    if (pipeline_object.handle == 0u)
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    pipelines_.erase(pipeline_object.handle);
}

void material_response_runtime_bridge::on_bind_pipeline(
    command_list *command_list,
    pipeline_stage stages,
    pipeline pipeline_object) noexcept
{
    if (g_replaying_draw ||
        command_list == nullptr ||
        (static_cast<std::uint32_t>(stages) &
         static_cast<std::uint32_t>(
             pipeline_stage::pixel_shader)) == 0u)
        return;

    const auto key =
        reinterpret_cast<std::uintptr_t>(
            command_list);

    std::lock_guard<std::mutex> lock(mutex_);

    const auto found =
        pipelines_.find(pipeline_object.handle);

    if (found == pipelines_.end()) {
        bound_by_command_.erase(key);
        g_draw_donor = -1;
        return;
    }

    bound_by_command_[key] = found->second;
    ++target_binds_;
}

bool material_response_runtime_bridge::on_draw_indexed(
    command_list *command_list,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept
{
    if (g_replaying_draw)
        return false;

    const int donor_index = g_draw_donor;
    g_draw_donor = -1;

    if (!features_.enabled(
            core::operator_id::material_response) ||
        !hooks_active_.load() ||
        quarantined_.load() ||
        command_list == nullptr ||
        donor_index < 0 ||
        static_cast<std::size_t>(donor_index) >=
            dsrrl::materialdonor::k_donors.size())
        return false;

    pipeline_record record;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto found =
            bound_by_command_.find(
                reinterpret_cast<std::uintptr_t>(
                    command_list));

        if (found == bound_by_command_.end())
            return false;

        record = found->second;
    }

    if (record.host_index >=
        g_device.alternates.size()) {
        ++fail_open_;
        return false;
    }

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            command_list->get_native());

    if (context == nullptr) {
        ++fail_open_;
        return false;
    }

    if (context->GetType() !=
        D3D11_DEVICE_CONTEXT_IMMEDIATE) {
        ++deferred_fail_open_;
        return false;
    }

    ID3D11Device *device = nullptr;
    context->GetDevice(&device);

    if (device == nullptr) {
        ++fail_open_;
        return false;
    }

    const auto &donor =
        dsrrl::materialdonor::k_donors[
            static_cast<std::size_t>(donor_index)];

    ID3D11PixelShader *replacement = nullptr;

    {
        std::lock_guard<std::mutex> lock(g_device_mutex);

        if (g_device.device == device) {
            const auto &pair =
                g_device.alternates[
                    record.host_index];

            replacement =
                donor.has_c101
                    ? pair.full
                    : pair.diffuse;

            if (replacement != nullptr)
                replacement->AddRef();
        }
    }

    ID3D11Buffer *b12 =
        realize_b12(
            device,
            donor_index);

    ID3D11PixelShader *old_shader = nullptr;
    context->PSGetShader(
        &old_shader,
        nullptr,
        nullptr);

    if (replacement == nullptr ||
        b12 == nullptr ||
        old_shader == nullptr) {
        if (replacement != nullptr)
            replacement->Release();
        if (b12 != nullptr)
            b12->Release();
        if (old_shader != nullptr)
            old_shader->Release();
        device->Release();
        ++fail_open_;
        return false;
    }

    ID3D11DeviceContext1 *context1 = nullptr;
    static_cast<void>(
        context->QueryInterface(
            __uuidof(ID3D11DeviceContext1),
            reinterpret_cast<void **>(
                &context1)));

    auto old_b12 =
        capture_b12(
            context,
            context1);

    if (!old_b12.coherent) {
        release_capture(old_b12);
        if (context1 != nullptr)
            context1->Release();
        replacement->Release();
        b12->Release();
        old_shader->Release();
        device->Release();
        ++fail_open_;
        return false;
    }

    context->PSSetShader(
        replacement,
        nullptr,
        0u);

    context->PSSetConstantBuffers(
        12u,
        1u,
        &b12);

    g_replaying_draw = true;

    if (instance_count <= 1u) {
        context->DrawIndexed(
            index_count,
            first_index,
            vertex_offset);
    } else {
        context->DrawIndexedInstanced(
            index_count,
            instance_count,
            first_index,
            vertex_offset,
            first_instance);
    }

    g_replaying_draw = false;

    context->PSSetShader(
        old_shader,
        nullptr,
        0u);

    restore_b12(
        context,
        context1,
        old_b12);

    release_capture(old_b12);

    if (context1 != nullptr)
        context1->Release();

    replacement->Release();
    b12->Release();
    old_shader->Release();
    device->Release();

    ++replay_draws_;

    if (donor.has_c101)
        ++c101_draws_;
    else
        ++c100_only_draws_;

    return true;
}

material_response_runtime_telemetry
material_response_runtime_bridge::telemetry() const noexcept
{
    return {
        g_mtd_seen.load(),
        g_donor_registered.load(),
        g_donor_unmapped.load(),
        g_selector_seen.load(),
        g_selector_donor.load(),
        exact_pipeline_hits_.load(),
        alternate_pairs_ready_.load(),
        target_binds_.load(),
        replay_draws_.load(),
        c101_draws_.load(),
        c100_only_draws_.load(),
        g_b12_created.load(),
        g_b12_cache_hits.load(),
        deferred_fail_open_.load(),
        fail_open_.load(),
        hooks_active_.load(),
        quarantined_.load()
    };
}

} // namespace dsrrl::runtime
