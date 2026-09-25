#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/asset_bridges.hpp"
#include "dsrrl/runtime/engine_hooks.hpp"
#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/generated_normal_routes_v12.hpp"
#include "dsrrl/runtime/generated_diffuse_routes_v12.hpp"
#include "dsrrl/runtime/generated_spec_routes_v12.hpp"

#include <reshade.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace reshade::api;

namespace dsrrl::runtime::assets {
namespace {

enum class asset_class : std::uint8_t {
    specular = 0,
    diffuse,
    normal
};

enum class load_status : std::uint8_t {
    missing = 0,
    ready,
    unsupported,
    create_failed
};

struct companion_set {
    device *owner_device = nullptr;
    std::uint64_t logical_hash = 0;
    ID3D11ShaderResourceView *specular = nullptr;
    ID3D11ShaderResourceView *diffuse = nullptr;
    ID3D11ShaderResourceView *normal = nullptr;
};

struct load_result {
    ID3D11ShaderResourceView *view = nullptr;
    load_status status = load_status::missing;
};

#pragma pack(push, 1)
struct dds_pixel_format {
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t fourcc;
    std::uint32_t rgb_bit_count;
    std::uint32_t r_mask;
    std::uint32_t g_mask;
    std::uint32_t b_mask;
    std::uint32_t a_mask;
};

struct dds_header {
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t height;
    std::uint32_t width;
    std::uint32_t pitch_or_linear_size;
    std::uint32_t depth;
    std::uint32_t mip_count;
    std::uint32_t reserved1[11];
    dds_pixel_format pixel_format;
    std::uint32_t caps;
    std::uint32_t caps2;
    std::uint32_t caps3;
    std::uint32_t caps4;
    std::uint32_t reserved2;
};

struct dds_header_dx10 {
    std::uint32_t dxgi_format;
    std::uint32_t resource_dimension;
    std::uint32_t misc_flag;
    std::uint32_t array_size;
    std::uint32_t misc_flags2;
};
#pragma pack(pop)

static_assert(sizeof(dds_pixel_format) == 32);
static_assert(sizeof(dds_header) == 124);
static_assert(sizeof(dds_header_dx10) == 20);

constexpr std::uint32_t k_dds_magic = 0x20534444u;
constexpr std::uint32_t k_fourcc_dxt1 = 0x31545844u;
constexpr std::uint32_t k_fourcc_dxt3 = 0x33545844u;
constexpr std::uint32_t k_fourcc_dxt5 = 0x35545844u;
constexpr std::uint32_t k_fourcc_dx10 = 0x30315844u;
constexpr std::uint32_t k_resource_dimension_texture2d = 3u;
constexpr std::uint32_t k_misc_texturecube = 0x4u;
constexpr std::uint32_t k_caps2_cubemap = 0x200u;

core::renderer_core *g_core = nullptr;
std::mutex g_cache_mutex;
std::unordered_map<std::uint64_t, companion_set> g_cache;

thread_local std::wstring g_logical_name;
thread_local bool g_internal_create = false;

std::atomic<std::uint64_t> g_name_capture{0};
std::atomic<std::uint64_t> g_name_clear{0};
std::atomic<std::uint64_t> g_named_srv{0};

std::atomic<std::uint64_t> g_spec_ready{0};
std::atomic<std::uint64_t> g_diff_ready{0};
std::atomic<std::uint64_t> g_norm_ready{0};
std::atomic<std::uint64_t> g_spec_missing{0};
std::atomic<std::uint64_t> g_diff_missing{0};
std::atomic<std::uint64_t> g_norm_missing{0};
std::atomic<std::uint64_t> g_unsupported{0};
std::atomic<std::uint64_t> g_create_fail{0};

std::atomic<std::uint64_t> g_spec_gate{0};
std::atomic<std::uint64_t> g_diff_gate{0};
std::atomic<std::uint64_t> g_norm_gate{0};
std::atomic<std::uint64_t> g_diff_pair_reject{0};
std::atomic<std::uint64_t> g_norm_tuple_reject{0};
std::atomic<std::uint64_t> g_spec_bind{0};
std::atomic<std::uint64_t> g_diff_bind{0};
std::atomic<std::uint64_t> g_norm_bind{0};
std::atomic<std::uint64_t> g_spec_restore{0};
std::atomic<std::uint64_t> g_diff_restore{0};
std::atomic<std::uint64_t> g_norm_restore{0};
std::atomic<std::uint64_t> g_restore_fail{0};
std::atomic<std::uint64_t> g_present{0};
std::atomic<bool> g_quarantined{false};

std::atomic<bool> g_first_spec{false};
std::atomic<bool> g_first_diff{false};
std::atomic<bool> g_first_norm{false};

std::uint64_t fnv_name(const std::wstring &name) noexcept
{
    std::uint64_t h = 14695981039346656037ull;
    for (wchar_t ch : name) {
        std::uint32_t c = static_cast<std::uint32_t>(ch);
        if (c >= static_cast<std::uint32_t>(L'A') &&
            c <= static_cast<std::uint32_t>(L'Z'))
            c += 32u;
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ull;
    }
    return h;
}

void release_view(ID3D11ShaderResourceView *&view) noexcept
{
    if (view != nullptr) {
        view->Release();
        view = nullptr;
    }
}

void release_set(companion_set &set) noexcept
{
    release_view(set.specular);
    release_view(set.diffuse);
    release_view(set.normal);
}

void release_cache_for_device(device *owner) noexcept
{
    if(owner==nullptr)
        return;
    try {
        std::vector<companion_set> dead;
        {
            std::lock_guard lock(g_cache_mutex);
            for(auto it=g_cache.begin();it!=g_cache.end();){
                if(it->second.owner_device==owner){
                    dead.push_back(it->second);
                    it=g_cache.erase(it);
                }else{
                    ++it;
                }
            }
        }
        for(auto &set:dead)
            release_set(set);
    } catch (...) {
        // Device teardown is fail-open. Remaining references are released
        // during addon unregister rather than risking an exception at ABI edge.
    }
}

void release_cache() noexcept
{
    // Detach ownership under the mutex, but never execute COM Release while
    // holding it: final SRV destruction may re-enter destroy_resource_view.
    // unordered_map::swap is non-allocating for equal allocators.
    try {
        std::unordered_map<std::uint64_t, companion_set> dead;
        {
            std::lock_guard lock(g_cache_mutex);
            dead.swap(g_cache);
        }
        for(auto &[_,set]:dead)
            release_set(set);
    } catch (...) {
        // Fail open during teardown. Leaking a sidecar reference is preferable
        // to propagating an exception across the ReShade/host callback ABI.
    }
}

std::filesystem::path process_dir()
{
    std::wstring buffer(32768, L'\0');
    const DWORD n = GetModuleFileNameW(
        nullptr,
        buffer.data(),
        static_cast<DWORD>(buffer.size()));

    if (n == 0 || n >= buffer.size())
        return {};

    buffer.resize(n);
    return std::filesystem::path(buffer).parent_path();
}

const wchar_t *class_dir(asset_class cls) noexcept
{
    switch (cls) {
    case asset_class::specular: return L"Specular";
    case asset_class::diffuse: return L"Diffuse";
    case asset_class::normal: return L"Normals";
    }
    return L"";
}

std::filesystem::path sidecar_path(
    asset_class cls,
    const std::wstring &logical_name)
{
    auto root = process_dir();
    if (root.empty() || logical_name.empty())
        return {};

    std::filesystem::path leaf(logical_name);
    std::wstring filename = leaf.filename().wstring();

    if (filename.size() < 4 ||
        _wcsicmp(filename.c_str() + filename.size() - 4, L".dds") != 0)
        filename += L".dds";

    return root / L"DSRRL" / class_dir(cls) / filename;
}

template<typename T>
bool read_struct(
    const std::vector<std::uint8_t> &bytes,
    std::size_t offset,
    T &out) noexcept
{
    if (offset > bytes.size() ||
        sizeof(T) > bytes.size() - offset)
        return false;

    std::memcpy(&out, bytes.data() + offset, sizeof(T));
    return true;
}

load_result load_dds(
    ID3D11Device *device,
    const std::filesystem::path &path) noexcept
{
    if (device == nullptr || path.empty())
        return {};

    try {
        if (!std::filesystem::is_regular_file(path))
            return {};

        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return {};

        stream.seekg(0, std::ios::end);
        const auto end = stream.tellg();
        if (end <= 0)
            return {nullptr, load_status::unsupported};

        const auto size = static_cast<std::size_t>(end);
        stream.seekg(0, std::ios::beg);

        std::vector<std::uint8_t> bytes(size);
        if (!stream.read(
                reinterpret_cast<char *>(bytes.data()),
                static_cast<std::streamsize>(bytes.size())))
            return {nullptr, load_status::unsupported};

        std::uint32_t magic = 0;
        dds_header header{};
        if (!read_struct(bytes, 0, magic) ||
            magic != k_dds_magic ||
            !read_struct(bytes, 4, header) ||
            header.size != 124u ||
            header.pixel_format.size != 32u ||
            header.width == 0u ||
            header.height == 0u)
            return {nullptr, load_status::unsupported};

        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        std::size_t data_offset = 4u + sizeof(dds_header);

        if (header.pixel_format.fourcc == k_fourcc_dxt1) {
            format = DXGI_FORMAT_BC1_UNORM;
        } else if (header.pixel_format.fourcc == k_fourcc_dxt3) {
            format = DXGI_FORMAT_BC2_UNORM;
        } else if (header.pixel_format.fourcc == k_fourcc_dxt5) {
            format = DXGI_FORMAT_BC3_UNORM;
        } else if (header.pixel_format.fourcc == k_fourcc_dx10) {
            dds_header_dx10 dx10{};
            if (!read_struct(bytes, data_offset, dx10))
                return {nullptr, load_status::unsupported};

            data_offset += sizeof(dx10);

            if (dx10.resource_dimension !=
                    k_resource_dimension_texture2d ||
                dx10.array_size != 1u ||
                (dx10.misc_flag & k_misc_texturecube) != 0u)
                return {nullptr, load_status::unsupported};

            // Material sidecars are authored for the renderer's explicit
            // shader-domain transfer. Binding an *_SRGB SRV would add an
            // implicit hardware decode and double-transform the material
            // signal. BC7 remains supported by the certified SpecRGB loader,
            // but only in the non-sRGB view class.
            switch (static_cast<DXGI_FORMAT>(dx10.dxgi_format)) {
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC7_UNORM:
                format = static_cast<DXGI_FORMAT>(dx10.dxgi_format);
                break;
            default:
                return {nullptr, load_status::unsupported};
            }
        } else {
            return {nullptr, load_status::unsupported};
        }

        if ((header.caps2 & k_caps2_cubemap) != 0u)
            return {nullptr, load_status::unsupported};

        const std::uint32_t mip_count =
            std::max(1u, header.mip_count);

        const std::uint32_t block_bytes =
            (format == DXGI_FORMAT_BC1_UNORM ||
             format == DXGI_FORMAT_BC1_UNORM_SRGB)
                ? 8u
                : 16u;

        if(header.width>16384u || header.height>16384u || header.depth>1u)
            return {nullptr,load_status::unsupported};
        std::uint32_t max_mips=1u;
        for(auto extent=std::max(header.width,header.height);extent>1u;extent>>=1u)
            ++max_mips;
        if(mip_count>max_mips)
            return {nullptr,load_status::unsupported};

        std::vector<D3D11_SUBRESOURCE_DATA> initial;
        initial.reserve(mip_count);

        std::size_t cursor = data_offset;
        std::uint32_t width = header.width;
        std::uint32_t height = header.height;

        for (std::uint32_t mip = 0;
             mip < mip_count;
             ++mip) {
            const std::uint32_t blocks_w =
                std::max(1u, (width + 3u) / 4u);
            const std::uint32_t blocks_h =
                std::max(1u, (height + 3u) / 4u);

            const std::uint32_t row_pitch =
                blocks_w * block_bytes;
            const std::uint32_t slice_pitch =
                row_pitch * blocks_h;

            if (cursor > bytes.size() ||
                slice_pitch > bytes.size() - cursor)
                return {nullptr, load_status::unsupported};

            D3D11_SUBRESOURCE_DATA sub{};
            sub.pSysMem = bytes.data() + cursor;
            sub.SysMemPitch = row_pitch;
            sub.SysMemSlicePitch = slice_pitch;
            initial.push_back(sub);

            cursor += slice_pitch;
            width = std::max(1u, width >> 1u);
            height = std::max(1u, height >> 1u);
        }

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = header.width;
        desc.Height = header.height;
        desc.MipLevels = mip_count;
        desc.ArraySize = 1u;
        desc.Format = format;
        desc.SampleDesc.Count = 1u;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        ID3D11Texture2D *texture = nullptr;
        ID3D11ShaderResourceView *view = nullptr;

        g_internal_create = true;
        const HRESULT create_texture =
            device->CreateTexture2D(
                &desc,
                initial.data(),
                &texture);

        HRESULT create_view = E_FAIL;
        if (SUCCEEDED(create_texture) &&
            texture != nullptr)
            create_view =
                device->CreateShaderResourceView(
                    texture,
                    nullptr,
                    &view);
        g_internal_create = false;

        if (texture != nullptr)
            texture->Release();

        if (FAILED(create_texture) ||
            FAILED(create_view) ||
            view == nullptr) {
            if (view != nullptr)
                view->Release();
            return {nullptr, load_status::create_failed};
        }

        return {view, load_status::ready};
    } catch (...) {
        g_internal_create = false;
        return {nullptr, load_status::create_failed};
    }
}

void account_load(
    asset_class cls,
    load_status status) noexcept
{
    if (status == load_status::unsupported) {
        ++g_unsupported;
        return;
    }

    if (status == load_status::create_failed) {
        ++g_create_fail;
        return;
    }

    if (cls == asset_class::specular) {
        if (status == load_status::ready) ++g_spec_ready;
        else ++g_spec_missing;
    } else if (cls == asset_class::diffuse) {
        if (status == load_status::ready) ++g_diff_ready;
        else ++g_diff_missing;
    } else {
        if (status == load_status::ready) ++g_norm_ready;
        else ++g_norm_missing;
    }
}

ID3D11ShaderResourceView *lookup(
    ID3D11ShaderResourceView *stock,
    asset_class cls) noexcept
{
    if (stock == nullptr)
        return nullptr;

    const auto key =
        static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(stock));

    std::lock_guard lock(g_cache_mutex);
    const auto it = g_cache.find(key);
    if (it == g_cache.end())
        return nullptr;

    ID3D11ShaderResourceView *result = nullptr;
    switch (cls) {
    case asset_class::specular: result = it->second.specular; break;
    case asset_class::diffuse: result = it->second.diffuse; break;
    case asset_class::normal: result = it->second.normal; break;
    }

    if (result != nullptr)
        result->AddRef();

    return result;
}

std::uint64_t logical_hash_for(ID3D11ShaderResourceView *stock) noexcept
{
    if (stock == nullptr)
        return 0;

    const auto key = static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(stock));

    std::lock_guard lock(g_cache_mutex);
    const auto it = g_cache.find(key);
    return it == g_cache.end() ? 0ull : it->second.logical_hash;
}

bool receiver_allowed(
    const material_route_scope &route,
    std::uint32_t receiver_id) noexcept
{
    return route.exact &&
        (route.receivers[0] == receiver_id ||
         route.receivers[1] == receiver_id ||
         route.receivers[2] == receiver_id);
}

void log_first_bind(
    asset_class cls,
    const material_route_scope &route,
    std::uint32_t receiver_id) noexcept
{
    try {
        std::atomic<bool> *flag = nullptr;
        const char *name = nullptr;

        switch (cls) {
        case asset_class::specular:
            flag = &g_first_spec; name = "SPEC"; break;
        case asset_class::diffuse:
            flag = &g_first_diff; name = "DIFF"; break;
        case asset_class::normal:
            flag = &g_first_norm; name = "NORMAL"; break;
        }

        bool expected = false;
        if (flag != nullptr &&
            flag->compare_exchange_strong(expected, true)) {
            std::ostringstream os;
            os << "[DSRRL A2 ASSET] FIRST_BIND island="
               << name
               << " route=" << route.route_index
               << " receiver=" << receiver_id;
            reshade::log::message(
                reshade::log::level::info,
                os.str().c_str());
        }
    } catch (...) {
        // Telemetry is non-authoritative. Never let logging break a draw.
    }
}

void release_draw_state(draw_state &state) noexcept
{
    release_view(state.old_t0);
    release_view(state.old_t1);
    release_view(state.old_t2);
    release_view(state.old_t10);
    state = {};
}

bool verify_slot(
    ID3D11DeviceContext *context,
    UINT slot,
    ID3D11ShaderResourceView *expected) noexcept
{
    ID3D11ShaderResourceView *got = nullptr;
    context->PSGetShaderResources(slot, 1u, &got);
    const bool ok = got == expected;
    if (got != nullptr)
        got->Release();
    return ok;
}

void on_init_resource_view(
    device *device,
    resource,
    resource_usage usage,
    const resource_view_desc &,
    resource_view view)
{
    if (g_internal_create ||
        g_quarantined.load() ||
        device == nullptr ||
        device->get_api() != device_api::d3d11 ||
        usage != resource_usage::shader_resource ||
        view.handle == 0u ||
        g_logical_name.empty())
        return;

    companion_set set{};
    try {
        const auto logical_hash = fnv_name(g_logical_name);
        const bool normal_member =
            generated::normal_name_hash_allowed_v12(logical_hash);
        const bool diffuse_member =
            generated::diffuse_name_hash_allowed_v12(logical_hash);
        const bool spec_member =
            generated::spec_name_hash_allowed_v12(logical_hash);

        // Exact V12 corpora are the authority. Unknown logical identities are
        // never associated with a bridge resource, even if a similarly named
        // DDS exists.
        if (!normal_member && !diffuse_member && !spec_member)
            return;

        auto *native =
            reinterpret_cast<ID3D11Device *>(device->get_native());
        if (native == nullptr)
            return;

        ++g_named_srv;
        set.owner_device = device;
        set.logical_hash = logical_hash;

        if (spec_member) {
            const auto spec =
                load_dds(native, sidecar_path(asset_class::specular, g_logical_name));
            account_load(asset_class::specular, spec.status);
            set.specular = spec.view;
        }

        if (generated::diffuse_target_hash_allowed_v12(logical_hash)) {
            const auto diff =
                load_dds(native, sidecar_path(asset_class::diffuse, g_logical_name));
            account_load(asset_class::diffuse, diff.status);
            set.diffuse = diff.view;
        }

        if (generated::normal_target_hash_allowed_v12(logical_hash)) {
            const auto normal =
                load_dds(native, sidecar_path(asset_class::normal, g_logical_name));
            account_load(asset_class::normal, normal.status);
            set.normal = normal.view;
        }

        const auto key = static_cast<std::uint64_t>(view.handle);
        companion_set old{};
        bool had_old = false;
        {
            std::lock_guard lock(g_cache_mutex);
            const auto it = g_cache.find(key);
            if (it != g_cache.end()) {
                old = it->second;
                it->second = set;
                had_old = true;
            } else {
                g_cache.emplace(key, set);
            }
        }

        // Ownership transferred to the cache.
        set = {};
        if (had_old)
            release_set(old);
    } catch (...) {
        release_set(set);
        ++g_create_fail;
        // Resource-side bridge discovery is optional. Any failure leaves the
        // stock SRV path untouched instead of escaping through the callback ABI.
    }
}
void on_destroy_resource_view(
    device *,
    resource_view view)
{
    companion_set dead{};
    bool found = false;

    {
        std::lock_guard lock(g_cache_mutex);
        const auto it =
            g_cache.find(
                static_cast<std::uint64_t>(
                    view.handle));

        if (it != g_cache.end()) {
            dead = it->second;
            g_cache.erase(it);
            found = true;
        }
    }

    if (found)
        release_set(dead);
}

void on_destroy_device(device *device_ptr)
{
    release_cache_for_device(device_ptr);
}

void on_present(
    command_queue *,
    swapchain *,
    const rect *,
    const rect *,
    std::uint32_t,
    const rect *)
{
    try {
        const auto n = ++g_present;
    if (n != 1u &&
        (n % 300u) != 0u)
        return;

    std::ostringstream os;
    os << "[DSRRL A2 ASSET] LIVE"
       << " names=" << g_name_capture.load()
       << " clears=" << g_name_clear.load()
       << " named_srv=" << g_named_srv.load()
       << " spec_ready=" << g_spec_ready.load()
       << " diff_ready=" << g_diff_ready.load()
       << " norm_ready=" << g_norm_ready.load()
       << " spec_gate=" << g_spec_gate.load()
       << " diff_gate=" << g_diff_gate.load()
       << " norm_gate=" << g_norm_gate.load()
       << " diff_pair_reject=" << g_diff_pair_reject.load()
       << " norm_tuple_reject=" << g_norm_tuple_reject.load()
       << " spec_bind=" << g_spec_bind.load()
       << " diff_bind=" << g_diff_bind.load()
       << " norm_bind=" << g_norm_bind.load()
       << " spec_restore=" << g_spec_restore.load()
       << " diff_restore=" << g_diff_restore.load()
       << " norm_restore=" << g_norm_restore.load()
       << " spec_missing=" << g_spec_missing.load()
       << " diff_missing=" << g_diff_missing.load()
       << " norm_missing=" << g_norm_missing.load()
       << " unsupported=" << g_unsupported.load()
       << " create_fail=" << g_create_fail.load()
       << " restore_fail=" << g_restore_fail.load()
       << " quarantine=" << (g_quarantined.load() ? 1 : 0);

        reshade::log::message(
            reshade::log::level::info,
            os.str().c_str());
    } catch (...) {
        // Telemetry must never cross the host callback ABI with an exception.
    }
}

} // namespace

void texture_name_event(
    const wchar_t *logical_name) noexcept
{
    g_logical_name.clear();

    if (logical_name == nullptr)
        return;

    try {
        constexpr std::size_t k_max = 512u;
        bool terminated = false;

        for (std::size_t i = 0;
             i < k_max;
             ++i) {
            wchar_t ch = L'\0';

            if (!engine::safe_read_bytes(
                    logical_name + i,
                    &ch,
                    sizeof(ch))) {
                g_logical_name.clear();
                return;
            }

            if (ch == L'\0') {
                terminated = true;
                break;
            }

            g_logical_name.push_back(ch);
        }

        // Logical texture identity is exact-only. Never admit a truncated
        // parser string as a valid resource key.
        if (!terminated) {
            g_logical_name.clear();
            return;
        }

        if (!g_logical_name.empty())
            ++g_name_capture;
    } catch (...) {
        g_logical_name.clear();
    }
}

void texture_name_clear_event() noexcept
{
    g_logical_name.clear();
    ++g_name_clear;
}

bool spec_ready(
    ID3D11DeviceContext *context,
    const material_route_scope &route,
    std::uint32_t receiver_id) noexcept
{
    if (context == nullptr ||
        g_core == nullptr ||
        g_quarantined.load() ||
        !receiver_allowed(route, receiver_id) ||
        !route.specular_material_verified ||
        receiver_id < 24u || receiver_id > 47u ||
        !g_core->features().enabled(core::operator_id::spec_rgb))
        return false;

    ID3D11ShaderResourceView *t1 = nullptr;
    context->PSGetShaderResources(1u, 1u, &t1);
    if (t1 == nullptr)
        return false;

    const auto h1 = logical_hash_for(t1);
    bool ready = h1 != 0u && generated::spec_name_hash_allowed_v12(h1);
    ID3D11ShaderResourceView *replacement = nullptr;
    if (ready)
        replacement = lookup(t1, asset_class::specular);
    ready = replacement != nullptr;
    if (replacement) replacement->Release();
    t1->Release();
    return ready;
}

bool diffuse_ready(
    ID3D11DeviceContext *context,
    const material_route_scope &route,
    std::uint32_t receiver_id) noexcept
{
    if(context==nullptr || g_core==nullptr || g_quarantined.load() ||
       !receiver_allowed(route,receiver_id) ||
       !material_owner_authorized(route.owner_authorization) ||
       !route.diffuse_eligible ||
       receiver_id<24u || receiver_id>35u ||
       !g_core->features().enabled(core::operator_id::diffuse))
        return false;

    ID3D11ShaderResourceView *views[2]{};
    context->PSGetShaderResources(0u,1u,&views[0]);
    context->PSGetShaderResources(1u,1u,&views[1]);

    const auto h0=logical_hash_for(views[0]);
    const auto h1=logical_hash_for(views[1]);
    bool ready=h0!=0u && h1!=0u &&
        generated::diffuse_pair_allowed_v12(h1,h0);

    ID3D11ShaderResourceView *replacement=nullptr;
    if(ready)
        replacement=lookup(views[0],asset_class::diffuse);
    ready=replacement!=nullptr;

    if(replacement) replacement->Release();
    release_view(views[0]);
    release_view(views[1]);
    return ready;
}

bool normal_ready(
    ID3D11DeviceContext *context,
    const material_route_scope &route,
    std::uint32_t receiver_id) noexcept
{
    if(context==nullptr || g_core==nullptr || g_quarantined.load() ||
       !receiver_allowed(route,receiver_id) ||
       !material_owner_authorized(route.owner_authorization) ||
       !route.normal_eligible ||
       receiver_id<24u || receiver_id>35u ||
       !g_core->features().enabled(core::operator_id::normal))
        return false;

    ID3D11ShaderResourceView *views[3]{};
    context->PSGetShaderResources(0u,3u,views);

    const auto h0=logical_hash_for(views[0]);
    const auto h1=logical_hash_for(views[1]);
    const auto h2=logical_hash_for(views[2]);
    bool ready=h0!=0u && h1!=0u && h2!=0u &&
        generated::normal_tuple_allowed_v12(h0,h1,h2);

    ID3D11ShaderResourceView *replacement=nullptr;
    if(ready)
        replacement=lookup(views[2],asset_class::normal);
    ready=replacement!=nullptr;

    if(replacement) replacement->Release();
    for(auto *&view:views) release_view(view);
    return ready;
}

bool body_surface_ready(ID3D11DeviceContext *context) noexcept
{
    if(!context || !g_core || g_quarantined.load() ||
       !g_core->features().enabled(core::operator_id::spec_rgb) ||
       !g_core->features().enabled(core::operator_id::diffuse) ||
       !g_core->features().enabled(core::operator_id::normal)) return false;
    ID3D11ShaderResourceView *views[3]{};
    context->PSGetShaderResources(0,3,views);
    const auto h0=logical_hash_for(views[0]);
    const auto h1=logical_hash_for(views[1]);
    const auto h2=logical_hash_for(views[2]);
    bool ready=(h1==0x724f5fe11b342205ull || h1==0x777ede2aecc3d102ull) &&
        generated::diffuse_pair_allowed_v12(h1,h0) &&
        generated::normal_tuple_allowed_v12(h0,h1,h2) &&
        generated::spec_name_hash_allowed_v12(h1);
    if(ready){
        auto *diff=lookup(views[0],asset_class::diffuse);
        auto *spec=lookup(views[1],asset_class::specular);
        auto *norm=lookup(views[2],asset_class::normal);
        ready=diff && spec && norm;
        release_view(diff); release_view(spec); release_view(norm);
    }
    for(auto *&view:views) release_view(view);
    return ready;
}

bool apply_draw(
    ID3D11DeviceContext *context,
    const material_route_scope &route,
    std::uint32_t receiver_id,
    draw_state &state) noexcept
{
    state = {};

    if (context == nullptr ||
        g_core == nullptr ||
        g_quarantined.load() ||
        !receiver_allowed(route, receiver_id))
        return true;

    const bool want_spec =
        route.specular_material_verified &&
        route.spec_t10_consumer_active &&
        receiver_id >= 24u && receiver_id <= 47u &&
        g_core->features().enabled(core::operator_id::spec_rgb);

    const bool bmp_receiver = receiver_id >= 24u && receiver_id <= 35u;
    const bool owner_authorized =
        material_owner_authorized(route.owner_authorization);

    const bool want_diff =
        owner_authorized &&
        route.diffuse_eligible &&
        route.diffuse_c100_carrier_active &&
        bmp_receiver &&
        g_core->features().enabled(core::operator_id::diffuse);

    const bool want_norm =
        owner_authorized &&
        route.normal_eligible &&
        bmp_receiver &&
        g_core->features().enabled(core::operator_id::normal);

    if (!want_spec && !want_diff && !want_norm)
        return true;

    context->PSGetShaderResources(0u, 1u, &state.old_t0);
    context->PSGetShaderResources(1u, 1u, &state.old_t1);
    context->PSGetShaderResources(2u, 1u, &state.old_t2);
    context->PSGetShaderResources(10u, 1u, &state.old_t10);

    const auto h0 = logical_hash_for(state.old_t0);
    const auto h1 = logical_hash_for(state.old_t1);
    const auto h2 = logical_hash_for(state.old_t2);

    if (want_spec) {
        ++g_spec_gate;
        if (h1 == 0u || !generated::spec_name_hash_allowed_v12(h1)) {
            release_draw_state(state);
            return false;
        }
        ID3D11ShaderResourceView *replacement =
            lookup(state.old_t1, asset_class::specular);
        if (replacement == nullptr) {
            release_draw_state(state);
            return false;
        }
        context->PSSetShaderResources(10u, 1u, &replacement);
        replacement->Release();
        state.changed_t10 = true;
        ++g_spec_bind;
        log_first_bind(asset_class::specular, route, receiver_id);
    }

    if (want_diff) {
        ++g_diff_gate;
        if (h0 == 0u || h1 == 0u ||
            !generated::diffuse_pair_allowed_v12(h1, h0)) {
            ++g_diff_pair_reject;
        } else {
            ID3D11ShaderResourceView *replacement =
                lookup(state.old_t0, asset_class::diffuse);
            if (replacement != nullptr) {
                context->PSSetShaderResources(0u, 1u, &replacement);
                replacement->Release();
                state.changed_t0 = true;
                ++g_diff_bind;
                log_first_bind(asset_class::diffuse, route, receiver_id);
            }
        }
    }

    // Diffuse is a hard dependency of the FULL PTDE material-domain
    // shader class. A preflight/cache race must never degrade into
    // t0_DSR * c100_PTDE with the PTDE linear receiver. If the requested
    // PTDE t0 was not actually bound, fail the replay transaction and let
    // the caller restore state before the untouched stock draw executes.
    if (want_diff && !state.changed_t0)
        return false;

    if (want_norm) {
        ++g_norm_gate;
        if (h0 == 0u || h1 == 0u || h2 == 0u ||
            !generated::normal_tuple_allowed_v12(h0, h1, h2)) {
            ++g_norm_tuple_reject;
        } else {
            ID3D11ShaderResourceView *replacement =
                lookup(state.old_t2, asset_class::normal);
            if (replacement != nullptr) {
                context->PSSetShaderResources(2u, 1u, &replacement);
                replacement->Release();
                state.changed_t2 = true;
                ++g_norm_bind;
                log_first_bind(asset_class::normal, route, receiver_id);
            }
        }
    }

    if (want_norm && !state.changed_t2)
        return false;

    return true;
}
bool restore_draw(
    ID3D11DeviceContext *context,
    draw_state &state) noexcept
{
    if (context == nullptr) {
        release_draw_state(state);
        return false;
    }

    if (state.changed_t0)
        context->PSSetShaderResources(
            0u,
            1u,
            &state.old_t0);

    if (state.changed_t2)
        context->PSSetShaderResources(
            2u,
            1u,
            &state.old_t2);

    if (state.changed_t10)
        context->PSSetShaderResources(
            10u,
            1u,
            &state.old_t10);

    bool ok = true;

    if (state.changed_t0) {
        const bool restored =
            verify_slot(
                context,
                0u,
                state.old_t0);
        ok = ok && restored;
        if (restored)
            ++g_diff_restore;
    }

    if (state.changed_t2) {
        const bool restored =
            verify_slot(
                context,
                2u,
                state.old_t2);
        ok = ok && restored;
        if (restored)
            ++g_norm_restore;
    }

    if (state.changed_t10) {
        const bool restored =
            verify_slot(
                context,
                10u,
                state.old_t10);
        ok = ok && restored;
        if (restored)
            ++g_spec_restore;
    }

    release_draw_state(state);

    if (!ok) {
        ++g_restore_fail;
        g_quarantined.store(true);
        reshade::log::message(
            reshade::log::level::error,
            "[DSRRL A2 ASSET] resource restore mismatch; asset bridges quarantined.");
    }

    return ok;
}

bool register_runtime(
    core::renderer_core &core) noexcept
{
    g_core = &core;
    g_quarantined.store(false);

    reshade::register_event<
        reshade::addon_event::init_resource_view>(
            on_init_resource_view);

    reshade::register_event<
        reshade::addon_event::destroy_resource_view>(
            on_destroy_resource_view);

    reshade::register_event<
        reshade::addon_event::destroy_device>(
            on_destroy_device);

    reshade::register_event<
        reshade::addon_event::present>(
            on_present);

    return true;
}

void unregister_runtime() noexcept
{
    reshade::unregister_event<
        reshade::addon_event::present>(
            on_present);

    reshade::unregister_event<
        reshade::addon_event::destroy_device>(
            on_destroy_device);

    reshade::unregister_event<
        reshade::addon_event::destroy_resource_view>(
            on_destroy_resource_view);

    reshade::unregister_event<
        reshade::addon_event::init_resource_view>(
            on_init_resource_view);

    release_cache();
    g_logical_name.clear();
    g_core = nullptr;
}

} // namespace dsrrl::runtime::assets
