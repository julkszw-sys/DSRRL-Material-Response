#define NOMINMAX
#include "runtime_core_v2_reshade.hpp"

#include <reshade.hpp>

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace dsrrl::runtime_v2::virtual_envspec {
namespace {

constexpr std::uint32_t k_source_size = 32;
constexpr std::uint32_t k_faces = 6;
constexpr std::uint32_t k_slots = 4;
constexpr std::uint32_t k_probe_count = 342;
constexpr std::uint32_t k_source_bytes_per_cube =
    k_source_size * k_source_size * 4u * k_faces;
constexpr std::uint32_t k_target_size = 256;
constexpr std::uint32_t k_target_levels = 8;
constexpr std::uint32_t k_target_subresources = k_faces * k_target_levels;

constexpr char k_expected_pack_sha256[] =
    "c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3";

struct replacement_resource {
    reshade::api::device *device = nullptr;
    reshade::api::resource source{};
    reshade::api::resource virtual_resource{};
    reshade::api::resource_view virtual_view{};
    std::uint32_t probe_index = 0;
    std::uint32_t slot = 0;
};

std::mutex g_mutex;
std::vector<std::uint8_t> g_pack;
std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> g_slot2_hash_index;
std::unordered_map<std::uint64_t, replacement_resource> g_replacements_by_resource;
std::unordered_map<std::uint64_t, reshade::api::resource_view> g_replacements_by_view;

std::atomic<bool> g_pack_ready{false};
std::atomic<std::uint64_t> g_candidate_resources{0};
std::atomic<std::uint64_t> g_exact_slot2_matches{0};
std::atomic<std::uint64_t> g_virtual_created{0};
std::atomic<std::uint64_t> g_virtual_create_fail{0};
std::atomic<std::uint64_t> g_t12_replaced{0};
std::atomic<std::uint64_t> g_t14_replaced{0};
std::atomic<std::uint64_t> g_other_slot_replaced{0};
std::atomic<std::uint64_t> g_present_count{0};

thread_local bool g_internal_resource_create = false;
thread_local bool g_internal_rebind = false;

std::uint64_t fnv1a64(const std::uint8_t *data, std::size_t size) noexcept
{
    constexpr std::uint64_t offset = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offset;

    for (std::size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= prime;
    }

    return hash;
}

std::string hex_string(const std::uint8_t *bytes, std::size_t size)
{
    static constexpr char lut[] = "0123456789abcdef";
    std::string out(size * 2, '\0');

    for (std::size_t i = 0; i < size; ++i) {
        out[i * 2 + 0] = lut[(bytes[i] >> 4) & 0xF];
        out[i * 2 + 1] = lut[bytes[i] & 0xF];
    }

    return out;
}

bool sha256(const std::vector<std::uint8_t> &data, std::array<std::uint8_t, 32> &digest)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_size = 0;
    DWORD hash_size = 0;
    DWORD cb = 0;
    std::vector<std::uint8_t> object;

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status < 0)
        return false;

    status = BCryptGetProperty(
        algorithm,
        BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&object_size),
        sizeof(object_size),
        &cb,
        0);
    if (status < 0)
        goto cleanup;

    status = BCryptGetProperty(
        algorithm,
        BCRYPT_HASH_LENGTH,
        reinterpret_cast<PUCHAR>(&hash_size),
        sizeof(hash_size),
        &cb,
        0);
    if (status < 0 || hash_size != digest.size())
        goto cleanup;

    object.resize(object_size);

    status = BCryptCreateHash(
        algorithm,
        &hash,
        object.data(),
        static_cast<ULONG>(object.size()),
        nullptr,
        0,
        0);
    if (status < 0)
        goto cleanup;

    if (!data.empty()) {
        status = BCryptHashData(
            hash,
            const_cast<PUCHAR>(data.data()),
            static_cast<ULONG>(data.size()),
            0);
        if (status < 0)
            goto cleanup;
    }

    status = BCryptFinishHash(
        hash,
        digest.data(),
        static_cast<ULONG>(digest.size()),
        0);

cleanup:
    if (hash != nullptr)
        BCryptDestroyHash(hash);
    if (algorithm != nullptr)
        BCryptCloseAlgorithmProvider(algorithm, 0);

    return status >= 0;
}

std::filesystem::path pack_path_from_module(HMODULE addon_module)
{
    std::array<wchar_t, 32768> module_path{};
    const DWORD len = GetModuleFileNameW(
        addon_module,
        module_path.data(),
        static_cast<DWORD>(module_path.size()));

    std::filesystem::path root;
    if (len != 0 && len < module_path.size())
        root = std::filesystem::path(module_path.data()).parent_path();
    else
        root = std::filesystem::current_path();

    return root / L"DSRRL" / L"EnvSpec" / L"PackedGI" /
        L"PTDE_GI_ENVSPEC_PACK_RGBA.bin";
}

bool load_pack(HMODULE addon_module)
{
    const std::filesystem::path path = pack_path_from_module(addon_module);

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return false;

    const std::streamsize size = file.tellg();
    constexpr std::size_t expected_size =
        static_cast<std::size_t>(k_probe_count) * k_slots *
        k_source_bytes_per_cube;

    if (size != static_cast<std::streamsize>(expected_size))
        return false;

    file.seekg(0, std::ios::beg);
    g_pack.resize(expected_size);

    if (!file.read(
            reinterpret_cast<char *>(g_pack.data()),
            static_cast<std::streamsize>(g_pack.size()))) {
        g_pack.clear();
        return false;
    }

    std::array<std::uint8_t, 32> digest{};
    if (!sha256(g_pack, digest) ||
        hex_string(digest.data(), digest.size()) != k_expected_pack_sha256) {
        g_pack.clear();
        return false;
    }

    g_slot2_hash_index.clear();
    for (std::uint32_t probe = 0; probe < k_probe_count; ++probe) {
        const std::uint32_t packed_index = probe * k_slots + 2u;
        const std::uint8_t *cube =
            g_pack.data() +
            static_cast<std::size_t>(packed_index) * k_source_bytes_per_cube;

        g_slot2_hash_index[fnv1a64(cube, k_source_bytes_per_cube)]
            .push_back(packed_index);
    }

    g_pack_ready = true;
    return true;
}

bool extract_cube_bytes(
    const reshade::api::resource_desc &desc,
    const reshade::api::subresource_data *initial_data,
    std::vector<std::uint8_t> &out)
{
    if (initial_data == nullptr ||
        desc.type != reshade::api::resource_type::texture_2d ||
        desc.texture.width != k_source_size ||
        desc.texture.height != k_source_size ||
        desc.texture.depth_or_layers != k_faces ||
        desc.texture.levels != 1 ||
        desc.texture.samples != 1)
        return false;

    out.resize(k_source_bytes_per_cube);

    constexpr std::uint32_t tight_row = k_source_size * 4u;
    constexpr std::uint32_t face_size =
        k_source_size * k_source_size * 4u;

    for (std::uint32_t face = 0; face < k_faces; ++face) {
        const reshade::api::subresource_data &src = initial_data[face];
        if (src.data == nullptr)
            return false;

        const std::uint32_t row_pitch =
            src.row_pitch != 0 ? src.row_pitch : tight_row;
        if (row_pitch < tight_row)
            return false;

        const auto *src_bytes =
            static_cast<const std::uint8_t *>(src.data);
        std::uint8_t *dst =
            out.data() + static_cast<std::size_t>(face) * face_size;

        for (std::uint32_t y = 0; y < k_source_size; ++y) {
            std::memcpy(
                dst + static_cast<std::size_t>(y) * tight_row,
                src_bytes + static_cast<std::size_t>(y) * row_pitch,
                tight_row);
        }
    }

    return true;
}

std::optional<std::uint32_t> match_slot2_cube(
    const std::vector<std::uint8_t> &cube)
{
    if (!g_pack_ready.load() || cube.size() != k_source_bytes_per_cube)
        return std::nullopt;

    const auto hit = g_slot2_hash_index.find(
        fnv1a64(cube.data(), cube.size()));
    if (hit == g_slot2_hash_index.end())
        return std::nullopt;

    for (const std::uint32_t packed_index : hit->second) {
        const std::uint8_t *candidate =
            g_pack.data() +
            static_cast<std::size_t>(packed_index) * k_source_bytes_per_cube;

        if (std::memcmp(
                candidate,
                cube.data(),
                k_source_bytes_per_cube) == 0)
            return packed_index;
    }

    return std::nullopt;
}

struct float3 {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

struct float4 {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

float4 load_rgba(
    const std::vector<std::uint8_t> &cube,
    std::uint32_t face,
    std::uint32_t x,
    std::uint32_t y)
{
    const std::size_t face_size =
        static_cast<std::size_t>(k_source_size) * k_source_size * 4u;
    const std::size_t pixel =
        static_cast<std::size_t>(face) * face_size +
        (static_cast<std::size_t>(y) * k_source_size + x) * 4u;

    return float4{
        cube[pixel + 0] / 255.0f,
        cube[pixel + 1] / 255.0f,
        cube[pixel + 2] / 255.0f,
        cube[pixel + 3] / 255.0f};
}

float4 lerp4(const float4 &a, const float4 &b, float t)
{
    return float4{
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t};
}

float3 sample_ptde_decoded(
    const std::vector<std::uint8_t> &cube,
    std::uint32_t face,
    float x,
    float y)
{
    x = std::clamp(x, 0.0f, static_cast<float>(k_source_size - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(k_source_size - 1));

    const std::uint32_t x0 = static_cast<std::uint32_t>(std::floor(x));
    const std::uint32_t y0 = static_cast<std::uint32_t>(std::floor(y));
    const std::uint32_t x1 = std::min(x0 + 1, k_source_size - 1);
    const std::uint32_t y1 = std::min(y0 + 1, k_source_size - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);

    const float4 top = lerp4(
        load_rgba(cube, face, x0, y0),
        load_rgba(cube, face, x1, y0),
        tx);
    const float4 bottom = lerp4(
        load_rgba(cube, face, x0, y1),
        load_rgba(cube, face, x1, y1),
        tx);
    const float4 filtered = lerp4(top, bottom, ty);

    // PTDE legacy EnvSpec performs sampled RGB / sampled alpha after filtering.
    const float alpha = std::max(filtered.a, 1.0f / 255.0f);
    return float3{
        filtered.r / alpha,
        filtered.g / alpha,
        filtered.b / alpha};
}

std::uint32_t encode_ufloat(float value, std::uint32_t mantissa_bits)
{
    if (!(value > 0.0f))
        return 0;

    if (!std::isfinite(value))
        return (31u << mantissa_bits);

    int exponent = 0;
    const float fraction = std::frexp(value, &exponent);
    const int unbiased = exponent - 1;
    int target_exponent = unbiased + 15;
    const std::uint32_t mantissa_scale = 1u << mantissa_bits;

    if (target_exponent >= 31)
        return (30u << mantissa_bits) | (mantissa_scale - 1u);

    if (target_exponent <= 0) {
        const float scaled = std::ldexp(
            value,
            14 + static_cast<int>(mantissa_bits));
        const long rounded = std::lround(scaled);

        if (rounded <= 0)
            return 0;
        if (rounded >= static_cast<long>(mantissa_scale))
            return 1u << mantissa_bits;

        return static_cast<std::uint32_t>(rounded);
    }

    const float normalized = fraction * 2.0f - 1.0f;
    long mantissa = std::lround(
        normalized * static_cast<float>(mantissa_scale));

    if (mantissa >= static_cast<long>(mantissa_scale)) {
        mantissa = 0;
        ++target_exponent;
        if (target_exponent >= 31)
            return (30u << mantissa_bits) | (mantissa_scale - 1u);
    }

    return
        (static_cast<std::uint32_t>(target_exponent) << mantissa_bits) |
        static_cast<std::uint32_t>(mantissa);
}

std::uint32_t pack_r11g11b10(const float3 &v)
{
    const std::uint32_t r = encode_ufloat(v.r, 6);
    const std::uint32_t g = encode_ufloat(v.g, 6);
    const std::uint32_t b = encode_ufloat(v.b, 5);

    return r | (g << 11) | (b << 22);
}

bool create_virtual_cube(
    reshade::api::device *device,
    const std::vector<std::uint8_t> &source_cube,
    std::uint32_t probe_index,
    replacement_resource &out)
{
    std::array<std::vector<std::uint32_t>, k_target_subresources> storage;
    std::array<reshade::api::subresource_data, k_target_subresources> subresources{};

    for (std::uint32_t face = 0; face < k_faces; ++face) {
        for (std::uint32_t level = 0; level < k_target_levels; ++level) {
            const std::uint32_t size = k_target_size >> level;
            const std::uint32_t subresource = face * k_target_levels + level;
            std::vector<std::uint32_t> &pixels = storage[subresource];
            pixels.resize(static_cast<std::size_t>(size) * size);

            for (std::uint32_t y = 0; y < size; ++y) {
                const float source_y =
                    (static_cast<float>(y) + 0.5f) *
                        static_cast<float>(k_source_size) /
                        static_cast<float>(size) -
                    0.5f;

                for (std::uint32_t x = 0; x < size; ++x) {
                    const float source_x =
                        (static_cast<float>(x) + 0.5f) *
                            static_cast<float>(k_source_size) /
                            static_cast<float>(size) -
                        0.5f;

                    pixels[static_cast<std::size_t>(y) * size + x] =
                        pack_r11g11b10(
                            sample_ptde_decoded(
                                source_cube,
                                face,
                                source_x,
                                source_y));
                }
            }

            subresources[subresource].data = pixels.data();
            subresources[subresource].row_pitch = size * sizeof(std::uint32_t);
            subresources[subresource].slice_pitch =
                size * size * sizeof(std::uint32_t);
        }
    }

    const reshade::api::resource_desc desc(
        reshade::api::resource_type::texture_2d,
        k_target_size,
        k_target_size,
        k_faces,
        k_target_levels,
        reshade::api::format::r11g11b10_float,
        1,
        reshade::api::memory_heap::default_,
        reshade::api::resource_usage::shader_resource,
        reshade::api::resource_flags::cube_compatible);

    reshade::api::resource resource{};
    reshade::api::resource_view view{};

    g_internal_resource_create = true;

    const bool resource_ok = device->create_resource(
        desc,
        subresources.data(),
        reshade::api::resource_usage::shader_resource,
        &resource);

    bool view_ok = false;
    if (resource_ok) {
        const reshade::api::resource_view_desc view_desc(
            reshade::api::resource_view_type::texture_cube,
            reshade::api::format::r11g11b10_float,
            0,
            k_target_levels,
            0,
            k_faces);

        view_ok = device->create_resource_view(
            resource,
            reshade::api::resource_usage::shader_resource,
            view_desc,
            &view);
    }

    if (!view_ok && resource.handle != 0)
        device->destroy_resource(resource);

    g_internal_resource_create = false;

    if (!resource_ok || !view_ok)
        return false;

    out.device = device;
    out.virtual_resource = resource;
    out.virtual_view = view;
    out.probe_index = probe_index;
    out.slot = 2;
    return true;
}

void on_init_resource(
    reshade::api::device *device,
    const reshade::api::resource_desc &desc,
    const reshade::api::subresource_data *initial_data,
    reshade::api::resource_usage,
    reshade::api::resource source)
{
    if (g_internal_resource_create ||
        !g_pack_ready.load() ||
        source.handle == 0)
        return;

    {
        std::lock_guard lock(g_mutex);
        if (g_replacements_by_resource.find(source.handle) !=
            g_replacements_by_resource.end())
            return;
    }

    std::vector<std::uint8_t> cube;
    if (!extract_cube_bytes(desc, initial_data, cube))
        return;

    ++g_candidate_resources;

    const std::optional<std::uint32_t> packed_index =
        match_slot2_cube(cube);
    if (!packed_index.has_value())
        return;

    ++g_exact_slot2_matches;

    replacement_resource replacement{};
    replacement.source = source;

    const std::uint32_t probe_index = *packed_index / k_slots;
    if (!create_virtual_cube(device, cube, probe_index, replacement)) {
        ++g_virtual_create_fail;
        return;
    }

    {
        std::lock_guard lock(g_mutex);
        const auto [it, inserted] = g_replacements_by_resource.emplace(
            source.handle,
            replacement);

        if (!inserted) {
            g_internal_resource_create = true;
            device->destroy_resource_view(replacement.virtual_view);
            device->destroy_resource(replacement.virtual_resource);
            g_internal_resource_create = false;
            return;
        }
    }

    ++g_virtual_created;
}

void on_destroy_resource(
    reshade::api::device *device,
    reshade::api::resource source)
{
    if (g_internal_resource_create || source.handle == 0)
        return;

    replacement_resource replacement{};
    bool found = false;

    {
        std::lock_guard lock(g_mutex);
        const auto it = g_replacements_by_resource.find(source.handle);
        if (it != g_replacements_by_resource.end()) {
            replacement = it->second;
            g_replacements_by_resource.erase(it);
            found = true;
        }

        for (auto vit = g_replacements_by_view.begin();
             vit != g_replacements_by_view.end();) {
            if (vit->second == replacement.virtual_view)
                vit = g_replacements_by_view.erase(vit);
            else
                ++vit;
        }
    }

    if (!found)
        return;

    g_internal_resource_create = true;
    if (replacement.virtual_view.handle != 0)
        device->destroy_resource_view(replacement.virtual_view);
    if (replacement.virtual_resource.handle != 0)
        device->destroy_resource(replacement.virtual_resource);
    g_internal_resource_create = false;
}

void on_init_resource_view(
    reshade::api::device *,
    reshade::api::resource source,
    reshade::api::resource_usage usage_type,
    const reshade::api::resource_view_desc &,
    reshade::api::resource_view source_view)
{
    if (g_internal_resource_create ||
        source.handle == 0 ||
        source_view.handle == 0 ||
        (usage_type & reshade::api::resource_usage::shader_resource) ==
            reshade::api::resource_usage::undefined)
        return;

    std::lock_guard lock(g_mutex);
    const auto it = g_replacements_by_resource.find(source.handle);
    if (it == g_replacements_by_resource.end())
        return;

    g_replacements_by_view[source_view.handle] = it->second.virtual_view;
}

void on_destroy_resource_view(
    reshade::api::device *,
    reshade::api::resource_view source_view)
{
    if (g_internal_resource_create || source_view.handle == 0)
        return;

    std::lock_guard lock(g_mutex);
    g_replacements_by_view.erase(source_view.handle);
}

std::optional<reshade::api::resource_view> replacement_for(
    reshade::api::resource_view source_view)
{
    if (source_view.handle == 0)
        return std::nullopt;

    std::lock_guard lock(g_mutex);
    const auto it = g_replacements_by_view.find(source_view.handle);
    if (it == g_replacements_by_view.end())
        return std::nullopt;

    return it->second;
}

void on_push_descriptors(
    reshade::api::command_list *cmd_list,
    reshade::api::shader_stage stages,
    reshade::api::pipeline_layout layout,
    std::uint32_t param_index,
    const reshade::api::descriptor_table_update &update)
{
    if (g_internal_rebind ||
        (stages & reshade::api::shader_stage::pixel) !=
            reshade::api::shader_stage::pixel ||
        (update.type != reshade::api::descriptor_type::shader_resource_view &&
         update.type !=
             reshade::api::descriptor_type::sampler_with_resource_view) ||
        update.descriptors == nullptr)
        return;

    for (std::uint32_t i = 0; i < update.count; ++i) {
        const std::uint32_t slot = update.binding + i;
        if (slot != 12 && slot != 14)
            continue;

        if (update.type ==
            reshade::api::descriptor_type::shader_resource_view) {
            reshade::api::resource_view source_view =
                static_cast<const reshade::api::resource_view *>(
                    update.descriptors)[i];

            const auto virtual_view = replacement_for(source_view);
            if (!virtual_view.has_value())
                continue;

            reshade::api::resource_view replacement_view = *virtual_view;
            reshade::api::descriptor_table_update replacement_update = update;
            replacement_update.binding = slot;
            replacement_update.count = 1;
            replacement_update.descriptors = &replacement_view;

            g_internal_rebind = true;
            cmd_list->push_descriptors(
                stages,
                layout,
                param_index,
                replacement_update);
            g_internal_rebind = false;
        }
        else {
            reshade::api::sampler_with_resource_view descriptor =
                static_cast<const reshade::api::sampler_with_resource_view *>(
                    update.descriptors)[i];

            const auto virtual_view = replacement_for(descriptor.view);
            if (!virtual_view.has_value())
                continue;

            descriptor.view = *virtual_view;
            reshade::api::descriptor_table_update replacement_update = update;
            replacement_update.binding = slot;
            replacement_update.count = 1;
            replacement_update.descriptors = &descriptor;

            g_internal_rebind = true;
            cmd_list->push_descriptors(
                stages,
                layout,
                param_index,
                replacement_update);
            g_internal_rebind = false;
        }

        if (slot == 12)
            ++g_t12_replaced;
        else if (slot == 14)
            ++g_t14_replaced;
        else
            ++g_other_slot_replaced;
    }
}

void log_snapshot(const runtime_snapshot &snapshot)
{
    const std::uint64_t present = ++g_present_count;
    if (present != 1 && (present % 300) != 0)
        return;

    char line[1024] = {};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL VIRTUAL ENVSPEC V1] P=%llu PACK=%u candidates=%llu "
        "slot2_exact=%llu virtual=%llu create_fail=%llu "
        "t12=%llu t14=%llu LIVE res=%llu view=%llu cmd=%llu",
        static_cast<unsigned long long>(present),
        g_pack_ready.load() ? 1u : 0u,
        static_cast<unsigned long long>(g_candidate_resources.load()),
        static_cast<unsigned long long>(g_exact_slot2_matches.load()),
        static_cast<unsigned long long>(g_virtual_created.load()),
        static_cast<unsigned long long>(g_virtual_create_fail.load()),
        static_cast<unsigned long long>(g_t12_replaced.load()),
        static_cast<unsigned long long>(g_t14_replaced.load()),
        static_cast<unsigned long long>(snapshot.live_resources),
        static_cast<unsigned long long>(snapshot.live_views),
        static_cast<unsigned long long>(snapshot.live_commands));

    reshade::log::message(reshade::log::level::info, line);
}

void cleanup_virtual_resources()
{
    std::vector<replacement_resource> replacements;
    {
        std::lock_guard lock(g_mutex);
        replacements.reserve(g_replacements_by_resource.size());
        for (const auto &[_, replacement] : g_replacements_by_resource)
            replacements.push_back(replacement);

        g_replacements_by_view.clear();
        g_replacements_by_resource.clear();
    }

    g_internal_resource_create = true;
    for (const replacement_resource &replacement : replacements) {
        if (replacement.device == nullptr)
            continue;
        if (replacement.virtual_view.handle != 0)
            replacement.device->destroy_resource_view(
                replacement.virtual_view);
        if (replacement.virtual_resource.handle != 0)
            replacement.device->destroy_resource(
                replacement.virtual_resource);
    }
    g_internal_resource_create = false;
}

} // namespace
} // namespace dsrrl::runtime_v2::virtual_envspec

extern "C" __declspec(dllexport) const char *NAME =
    "DSRRL PTDE Virtual EnvSpec V1";
extern "C" __declspec(dllexport) const char *DESCRIPTION =
    "Diagnostic PTDE slot2 to DSR-topology 256x256 8-mip R11G11B10_FLOAT virtual EnvSpec carrier.";

extern "C" __declspec(dllexport) bool AddonInit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    if (!reshade::register_addon(addon_module, reshade_module))
        return false;

    using namespace dsrrl::runtime_v2;
    using namespace dsrrl::runtime_v2::virtual_envspec;

    if (!load_pack(addon_module)) {
        reshade::log::message(
            reshade::log::level::error,
            "[DSRRL VIRTUAL ENVSPEC V1] canonical PackedGI load/SHA failed; fail-open");
    }
    else {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL VIRTUAL ENVSPEC V1] canonical PackedGI PASS; slot2 virtualization armed");
    }

    set_snapshot_sink(log_snapshot);
    register_reshade_events();

    reshade::register_event<reshade::addon_event::init_resource>(
        on_init_resource);
    reshade::register_event<reshade::addon_event::destroy_resource>(
        on_destroy_resource);
    reshade::register_event<reshade::addon_event::init_resource_view>(
        on_init_resource_view);
    reshade::register_event<reshade::addon_event::destroy_resource_view>(
        on_destroy_resource_view);
    reshade::register_event<reshade::addon_event::push_descriptors>(
        on_push_descriptors);

    return true;
}

extern "C" __declspec(dllexport) void AddonUninit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    using namespace dsrrl::runtime_v2;
    using namespace dsrrl::runtime_v2::virtual_envspec;

    reshade::unregister_event<reshade::addon_event::push_descriptors>(
        on_push_descriptors);
    reshade::unregister_event<reshade::addon_event::destroy_resource_view>(
        on_destroy_resource_view);
    reshade::unregister_event<reshade::addon_event::init_resource_view>(
        on_init_resource_view);
    reshade::unregister_event<reshade::addon_event::destroy_resource>(
        on_destroy_resource);
    reshade::unregister_event<reshade::addon_event::init_resource>(
        on_init_resource);

    unregister_reshade_events();
    set_snapshot_sink(nullptr);
    cleanup_virtual_resources();

    reshade::unregister_addon(addon_module, reshade_module);
}
