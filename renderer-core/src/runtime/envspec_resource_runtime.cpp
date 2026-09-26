#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/envspec_resource_runtime.hpp"

#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <Windows.h>
#include <d3d11.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dsrrl::runtime {
namespace {

using namespace reshade::api;
namespace env = operators::env_spec;
namespace hashing = operators::legacy_plan::hashing;

constexpr std::uint32_t k_dsr_size = 256u;
constexpr std::uint32_t k_faces = 6u;
constexpr std::uint32_t k_dsr_levels = 8u;
constexpr std::size_t k_dsr_tight_bc6h_bytes = 524352u;

constexpr std::uint32_t k_ptde_size = 32u;
constexpr std::uint32_t k_ptde_slots = 4u;
constexpr std::size_t k_ptde_face_bytes =
    static_cast<std::size_t>(k_ptde_size) *
    k_ptde_size * 4u;
constexpr std::size_t k_ptde_cube_bytes =
    k_ptde_face_bytes * k_faces;

struct native_probe_record {
    std::uint16_t probe_ordinal = 0;
};

struct rgba_cube {
    resource texture{};
    resource_view view{};
};

std::mutex g_mutex;
std::unordered_map<std::uint64_t,native_probe_record>
    g_native_resources;
std::unordered_map<std::uint64_t,std::uint64_t>
    g_resource_by_view;
std::unordered_map<std::uint32_t,rgba_cube>
    g_ptde_cubes;

device *g_device = nullptr;
std::vector<std::uint8_t> g_pack;
sampler g_sampler{};
bool g_registered = false;
bool g_pack_ready = false;
bool g_sampler_ready = false;

std::atomic<std::uint64_t> g_native_candidates{0};
std::atomic<std::uint64_t> g_native_matches{0};
std::atomic<std::uint64_t> g_native_hash_miss{0};
std::atomic<std::uint64_t> g_view_matches{0};
std::atomic<std::uint64_t> g_pack_admit_ok{0};
std::atomic<std::uint64_t> g_pack_admit_fail{0};
std::atomic<std::uint64_t> g_cube_created{0};
std::atomic<std::uint64_t> g_cube_fail{0};
std::atomic<std::uint64_t> g_prepare_ok{0};
std::atomic<std::uint64_t> g_prepare_fail{0};

std::filesystem::path process_dir()
{
    std::wstring buffer(32768, L'\0');
    const DWORD size =
        GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size()));

    if (size == 0u ||
        size >= buffer.size())
        return {};

    buffer.resize(size);
    return std::filesystem::path(
        buffer).parent_path();
}

bool admit_pack()
{
    try {
        const auto root =
            process_dir();
        if (root.empty())
            return false;

        const auto path =
            root /
            std::filesystem::path(
                env::k_legacy_packed_gi_relative_path);

        std::error_code ec;
        const auto size =
            std::filesystem::file_size(
                path,
                ec);

        if (ec ||
            size != env::k_legacy_packed_gi_size)
            return false;

        std::ifstream stream(
            path,
            std::ios::binary);
        if (!stream)
            return false;

        std::vector<std::uint8_t> bytes(
            static_cast<std::size_t>(size));

        if (!stream.read(
                reinterpret_cast<char *>(
                    bytes.data()),
                static_cast<std::streamsize>(
                    bytes.size())))
            return false;

        const auto digest =
            hashing::sha256(
                bytes.data(),
                bytes.size());

        if (digest !=
            env::k_legacy_packed_gi_sha256)
            return false;

        g_pack =
            std::move(bytes);
        return true;
    } catch (...) {
        return false;
    }
}

bool is_native_envspec_desc(
    const resource_desc &desc) noexcept
{
    if (desc.type !=
            resource_type::texture_2d ||
        desc.texture.width != k_dsr_size ||
        desc.texture.height != k_dsr_size ||
        desc.texture.depth_or_layers != k_faces ||
        desc.texture.levels != k_dsr_levels ||
        desc.texture.samples != 1u)
        return false;

    return
        desc.texture.format ==
            format::bc6h_typeless ||
        desc.texture.format ==
            format::bc6h_ufloat ||
        desc.texture.format ==
            format::bc6h_sfloat;
}

bool extract_tight_bc6h(
    const resource_desc &desc,
    const subresource_data *initial_data,
    std::vector<std::uint8_t> &out)
{
    if (!is_native_envspec_desc(desc) ||
        initial_data == nullptr)
        return false;

    try {
        out.clear();
        out.reserve(
            k_dsr_tight_bc6h_bytes);

        for (std::uint32_t face = 0u;
             face < k_faces;
             ++face) {
            for (std::uint32_t level = 0u;
                 level < k_dsr_levels;
                 ++level) {
                const std::uint32_t sub =
                    face * k_dsr_levels +
                    level;
                const auto &src =
                    initial_data[sub];

                if (src.data == nullptr)
                    return false;

                const std::uint32_t width =
                    std::max(
                        1u,
                        k_dsr_size >> level);
                const std::uint32_t height =
                    std::max(
                        1u,
                        k_dsr_size >> level);
                const std::uint32_t block_cols =
                    std::max(
                        1u,
                        (width + 3u) / 4u);
                const std::uint32_t block_rows =
                    std::max(
                        1u,
                        (height + 3u) / 4u);
                const std::uint32_t tight_row =
                    block_cols * 16u;
                const std::uint32_t row_pitch =
                    src.row_pitch != 0u
                        ? src.row_pitch
                        : tight_row;

                if (row_pitch < tight_row)
                    return false;

                const auto *bytes =
                    static_cast<
                        const std::uint8_t *>(
                            src.data);

                for (std::uint32_t y = 0u;
                     y < block_rows;
                     ++y) {
                    const auto offset =
                        static_cast<std::size_t>(y) *
                        row_pitch;

                    out.insert(
                        out.end(),
                        bytes + offset,
                        bytes + offset +
                            tight_row);
                }
            }
        }
    } catch (...) {
        out.clear();
        return false;
    }

    return
        out.size() ==
        k_dsr_tight_bc6h_bytes;
}

void release_carrier(
    device *device_ptr) noexcept
{
    if (device_ptr == nullptr)
        return;

    std::unordered_map<
        std::uint32_t,
        rgba_cube> cubes;
    sampler sampler_to_destroy{};

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);

        if (g_device != device_ptr)
            return;

        cubes.swap(g_ptde_cubes);
        sampler_to_destroy =
            g_sampler;
        g_sampler = {};
        g_sampler_ready = false;
        g_pack_ready = false;
        g_pack.clear();

        g_native_resources.clear();
        g_resource_by_view.clear();
        g_device = nullptr;
    }

    for (const auto &[_,cube] : cubes) {
        if (cube.view.handle != 0u)
            device_ptr->destroy_resource_view(
                cube.view);
        if (cube.texture.handle != 0u)
            device_ptr->destroy_resource(
                cube.texture);
    }

    if (sampler_to_destroy.handle != 0u)
        device_ptr->destroy_sampler(
            sampler_to_destroy);
}

void on_init_device(
    device *device_ptr)
{
    if (device_ptr == nullptr ||
        device_ptr->get_api() !=
            device_api::d3d11)
        return;

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);

        if (g_device != nullptr &&
            g_device != device_ptr)
            return;

        g_device = device_ptr;
    }

    g_pack_ready =
        admit_pack();

    if (g_pack_ready)
        ++g_pack_admit_ok;
    else
        ++g_pack_admit_fail;

    sampler_desc desc{};
    desc.filter =
        filter_mode::anisotropic;
    desc.address_u =
        texture_address_mode::mirror;
    desc.address_v =
        texture_address_mode::mirror;
    desc.address_w =
        texture_address_mode::wrap;
    desc.max_anisotropy = 1.0f;
    desc.min_lod = 0.0f;
    desc.max_lod = 0.0f;

    g_sampler_ready =
        device_ptr->create_sampler(
            desc,
            &g_sampler);
}

void on_destroy_device(
    device *device_ptr)
{
    release_carrier(
        device_ptr);
}

void on_init_resource(
    device *device_ptr,
    const resource_desc &desc,
    const subresource_data *initial_data,
    resource_usage,
    resource resource_handle)
{
    if (device_ptr == nullptr ||
        resource_handle.handle == 0u ||
        !is_native_envspec_desc(desc))
        return;

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);
        if (device_ptr != g_device)
            return;
    }

    ++g_native_candidates;

    std::vector<std::uint8_t> tight;
    if (!extract_tight_bc6h(
            desc,
            initial_data,
            tight)) {
        ++g_native_hash_miss;
        return;
    }

    const auto digest =
        hashing::sha256(
            tight.data(),
            tight.size());

    const auto probe =
        env::legacy_native_probe_for_sha(
            digest);

    if (!probe.has_value()) {
        ++g_native_hash_miss;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);
        g_native_resources[
            resource_handle.handle] = {
                probe.value()
            };
    }

    ++g_native_matches;
}

void on_destroy_resource(
    device *device_ptr,
    resource resource_handle)
{
    if (device_ptr == nullptr ||
        resource_handle.handle == 0u)
        return;

    std::lock_guard<std::mutex> lock(
        g_mutex);

    if (device_ptr != g_device)
        return;

    g_native_resources.erase(
        resource_handle.handle);

    for (auto it =
             g_resource_by_view.begin();
         it != g_resource_by_view.end();) {
        if (it->second ==
            resource_handle.handle)
            it =
                g_resource_by_view.erase(
                    it);
        else
            ++it;
    }
}

void on_init_resource_view(
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

    if (device_ptr != g_device ||
        g_native_resources.find(
            resource_handle.handle) ==
            g_native_resources.end())
        return;

    g_resource_by_view[
        view.handle] =
        resource_handle.handle;

    ++g_view_matches;
}

void on_destroy_resource_view(
    device *device_ptr,
    resource_view view)
{
    if (device_ptr == nullptr ||
        view.handle == 0u)
        return;

    std::lock_guard<std::mutex> lock(
        g_mutex);

    if (device_ptr != g_device)
        return;

    g_resource_by_view.erase(
        view.handle);
}

bool probe_for_native_view(
    ID3D11ShaderResourceView *view,
    std::uint16_t &probe) noexcept
{
    probe = 0u;
    if (view == nullptr)
        return false;

    const auto key =
        static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(
                view));

    std::lock_guard<std::mutex> lock(
        g_mutex);

    const auto by_view =
        g_resource_by_view.find(key);
    if (by_view ==
        g_resource_by_view.end())
        return false;

    const auto by_resource =
        g_native_resources.find(
            by_view->second);

    if (by_resource ==
        g_native_resources.end())
        return false;

    probe =
        by_resource->second.probe_ordinal;
    return true;
}

bool get_ptde_cube(
    std::uint16_t probe,
    std::uint8_t slot,
    resource_view &view) noexcept
{
    view = {};

    if (probe >=
            env::k_legacy_envspec_probe_count ||
        slot >= k_ptde_slots)
        return false;

    device *device_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(
            g_mutex);

        if (g_device == nullptr ||
            !g_pack_ready)
            return false;

        device_ptr =
            g_device;

        const std::uint32_t key =
            static_cast<std::uint32_t>(
                probe) *
            k_ptde_slots +
            slot;

        const auto found =
            g_ptde_cubes.find(key);

        if (found !=
            g_ptde_cubes.end()) {
            view =
                found->second.view;
            return
                view.handle != 0u;
        }
    }

    const std::uint32_t key =
        static_cast<std::uint32_t>(
            probe) *
        k_ptde_slots +
        slot;

    const std::size_t offset =
        static_cast<std::size_t>(key) *
        k_ptde_cube_bytes;

    if (offset > g_pack.size() ||
        k_ptde_cube_bytes >
            g_pack.size() - offset) {
        ++g_cube_fail;
        return false;
    }

    std::array<subresource_data,k_faces>
        subresources{};

    for (std::uint32_t face = 0u;
         face < k_faces;
         ++face) {
        subresources[face].data =
            g_pack.data() +
            offset +
            static_cast<std::size_t>(face) *
                k_ptde_face_bytes;
        subresources[face].row_pitch =
            k_ptde_size * 4u;
        subresources[face].slice_pitch =
            static_cast<std::uint32_t>(
                k_ptde_face_bytes);
    }

    const resource_desc desc(
        resource_type::texture_2d,
        k_ptde_size,
        k_ptde_size,
        k_faces,
        1u,
        format::r8g8b8a8_unorm,
        1u,
        memory_heap::default_,
        resource_usage::shader_resource,
        resource_flags::cube_compatible);

    rgba_cube cube{};

    if (!device_ptr->create_resource(
            desc,
            subresources.data(),
            resource_usage::shader_resource,
            &cube.texture)) {
        ++g_cube_fail;
        return false;
    }

    const resource_view_desc view_desc(
        resource_view_type::texture_cube,
        format::r8g8b8a8_unorm,
        0u,
        1u,
        0u,
        k_faces);

    if (!device_ptr->create_resource_view(
            cube.texture,
            resource_usage::shader_resource,
            view_desc,
            &cube.view)) {
        device_ptr->destroy_resource(
            cube.texture);
        ++g_cube_fail;
        return false;
    }

    bool inserted = false;
    {
        std::lock_guard<std::mutex> lock(
            g_mutex);

        const auto result =
            g_ptde_cubes.emplace(
                key,
                cube);

        inserted =
            result.second;
        view =
            result.first->second.view;
    }

    if (!inserted) {
        device_ptr->destroy_resource_view(
            cube.view);
        device_ptr->destroy_resource(
            cube.texture);
    } else {
        ++g_cube_created;
    }

    return
        view.handle != 0u;
}

} // namespace

envspec_resource_runtime::~envspec_resource_runtime()
{
    unregister_events();
}

bool envspec_resource_runtime::register_events() noexcept
{
    if (g_registered)
        return true;

    try {
        reshade::register_event<
            reshade::addon_event::init_device>(
                on_init_device);
        reshade::register_event<
            reshade::addon_event::destroy_device>(
                on_destroy_device);
        reshade::register_event<
            reshade::addon_event::init_resource>(
                on_init_resource);
        reshade::register_event<
            reshade::addon_event::destroy_resource>(
                on_destroy_resource);
        reshade::register_event<
            reshade::addon_event::init_resource_view>(
                on_init_resource_view);
        reshade::register_event<
            reshade::addon_event::destroy_resource_view>(
                on_destroy_resource_view);

        g_registered = true;
        return true;
    } catch (...) {
        unregister_events();
        return false;
    }
}

void envspec_resource_runtime::unregister_events() noexcept
{
    if (!g_registered)
        return;

    reshade::unregister_event<
        reshade::addon_event::destroy_resource_view>(
            on_destroy_resource_view);
    reshade::unregister_event<
        reshade::addon_event::init_resource_view>(
            on_init_resource_view);
    reshade::unregister_event<
        reshade::addon_event::destroy_resource>(
            on_destroy_resource);
    reshade::unregister_event<
        reshade::addon_event::init_resource>(
            on_init_resource);
    reshade::unregister_event<
        reshade::addon_event::destroy_device>(
            on_destroy_device);
    reshade::unregister_event<
        reshade::addon_event::init_device>(
            on_init_device);

    device *device_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(
            g_mutex);
        device_ptr =
            g_device;
    }

    if (device_ptr != nullptr)
        release_carrier(
            device_ptr);

    g_registered = false;
}

bool envspec_resource_runtime::prepare(
    ID3D11DeviceContext *context,
    std::uint8_t slot,
    bool probe_b_required,
    prepared_envspec_resources &prepared) noexcept
{
    prepared = {};

    if (context == nullptr ||
        slot >= k_ptde_slots) {
        ++g_prepare_fail;
        return false;
    }

    ID3D11ShaderResourceView *stock_a =
        nullptr;
    ID3D11ShaderResourceView *stock_b =
        nullptr;

    context->PSGetShaderResources(
        12u,
        1u,
        &stock_a);
    context->PSGetShaderResources(
        14u,
        1u,
        &stock_b);

    std::uint16_t probe_a = 0u;
    std::uint16_t probe_b = 0u;

    const bool a_ok =
        probe_for_native_view(
            stock_a,
            probe_a);

    const bool b_ok =
        probe_for_native_view(
            stock_b,
            probe_b);

    if (stock_a != nullptr)
        stock_a->Release();
    if (stock_b != nullptr)
        stock_b->Release();

    if (!a_ok ||
        (probe_b_required && !b_ok)) {
        ++g_prepare_fail;
        return false;
    }

    if (!probe_b_required)
        probe_b = probe_a;

    resource_view a_view{};
    resource_view b_view{};

    if (!get_ptde_cube(
            probe_a,
            slot,
            a_view) ||
        !get_ptde_cube(
            probe_b,
            slot,
            b_view)) {
        ++g_prepare_fail;
        return false;
    }

    sampler sampler_handle{};
    {
        std::lock_guard<std::mutex> lock(
            g_mutex);

        if (!g_sampler_ready ||
            g_sampler.handle == 0u) {
            ++g_prepare_fail;
            return false;
        }

        sampler_handle =
            g_sampler;
    }

    auto *a_native =
        reinterpret_cast<
            ID3D11ShaderResourceView *>(
                static_cast<std::uintptr_t>(
                    a_view.handle));

    auto *b_native =
        reinterpret_cast<
            ID3D11ShaderResourceView *>(
                static_cast<std::uintptr_t>(
                    b_view.handle));

    auto *sampler_native =
        reinterpret_cast<
            ID3D11SamplerState *>(
                static_cast<std::uintptr_t>(
                    sampler_handle.handle));

    if (a_native == nullptr ||
        b_native == nullptr ||
        sampler_native == nullptr) {
        ++g_prepare_fail;
        return false;
    }

    a_native->AddRef();
    b_native->AddRef();
    sampler_native->AddRef();

    prepared.ptde_a =
        a_native;
    prepared.ptde_b =
        b_native;
    prepared.sampler =
        sampler_native;
    prepared.probe_a =
        probe_a;
    prepared.probe_b =
        probe_b;
    prepared.slot =
        slot;
    prepared.probe_b_required =
        probe_b_required;
    prepared.ready = true;

    ++g_prepare_ok;
    return true;
}

void envspec_resource_runtime::release(
    prepared_envspec_resources &prepared) noexcept
{
    if (prepared.ptde_a != nullptr)
        prepared.ptde_a->Release();
    if (prepared.ptde_b != nullptr)
        prepared.ptde_b->Release();
    if (prepared.sampler != nullptr)
        prepared.sampler->Release();

    prepared = {};
}

envspec_resource_telemetry
envspec_resource_runtime::telemetry() const noexcept
{
    return {
        g_native_candidates.load(),
        g_native_matches.load(),
        g_native_hash_miss.load(),
        g_view_matches.load(),
        g_pack_admit_ok.load(),
        g_pack_admit_fail.load(),
        g_cube_created.load(),
        g_cube_fail.load(),
        g_prepare_ok.load(),
        g_prepare_fail.load(),
        g_pack_ready,
        g_sampler_ready
    };
}

void envspec_resource_runtime::reset_stats() noexcept
{
    g_native_candidates.store(0u);
    g_native_matches.store(0u);
    g_native_hash_miss.store(0u);
    g_view_matches.store(0u);
    g_pack_admit_ok.store(0u);
    g_pack_admit_fail.store(0u);
    g_cube_created.store(0u);
    g_cube_fail.store(0u);
    g_prepare_ok.store(0u);
    g_prepare_fail.store(0u);
}

} // namespace dsrrl::runtime
