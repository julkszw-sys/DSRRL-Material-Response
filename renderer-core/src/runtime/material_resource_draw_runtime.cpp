#include "dsrrl/runtime/material_resource_draw_runtime.hpp"
#include "dsrrl/runtime/texture_identity_transport.hpp"
#include "dsrrl/operators/resource_bridges/spec_rgb_bridge.hpp"
#include "dsrrl/operators/resource_bridges/diffuse_bridge.hpp"
#include "dsrrl/operators/resource_bridges/normal_bridge.hpp"
#include "dsrrl/runtime/generated_spec_routes_v12.hpp"
#include "dsrrl/runtime/generated_diffuse_routes_v12.hpp"
#include "dsrrl/runtime/generated_normal_routes_v12.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d11.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace dsrrl::runtime {
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
    ID3D11Device *device = nullptr;
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

static_assert(sizeof(dds_pixel_format) == 32u);
static_assert(sizeof(dds_header) == 124u);
static_assert(sizeof(dds_header_dx10) == 20u);

constexpr std::uint32_t k_dds_magic = 0x20534444u;
constexpr std::uint32_t k_fourcc_dxt1 = 0x31545844u;
constexpr std::uint32_t k_fourcc_dxt3 = 0x33545844u;
constexpr std::uint32_t k_fourcc_dxt5 = 0x35545844u;
constexpr std::uint32_t k_fourcc_dx10 = 0x30315844u;
constexpr std::uint32_t k_resource_dimension_texture2d = 3u;
constexpr std::uint32_t k_misc_texturecube = 0x4u;
constexpr std::uint32_t k_caps2_cubemap = 0x200u;

core::renderer_core *g_core = nullptr;
std::mutex g_mutex;
std::unordered_map<std::uint64_t, companion_set> g_cache;
thread_local bool g_internal_create = false;

std::atomic<std::uint64_t> g_named_views{0};
std::atomic<std::uint64_t> g_sidecar_ready{0};
std::atomic<std::uint64_t> g_sidecar_missing{0};
std::atomic<std::uint64_t> g_sidecar_unsupported{0};
std::atomic<std::uint64_t> g_spec_requests{0};
std::atomic<std::uint64_t> g_diffuse_requests{0};
std::atomic<std::uint64_t> g_normal_requests{0};
std::atomic<std::uint64_t> g_fail_open{0};
std::atomic_bool g_quarantined{false};

std::uint64_t fnv_name(const std::wstring &name) noexcept
{
    std::uint64_t h = 14695981039346656037ull;

    for (wchar_t ch : name) {
        std::uint32_t c =
            static_cast<std::uint32_t>(ch);

        if (c >= static_cast<std::uint32_t>(L'A') &&
            c <= static_cast<std::uint32_t>(L'Z'))
            c += 32u;

        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ull;
    }

    return h;
}

void release_view(
    ID3D11ShaderResourceView *&view) noexcept
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

    if (set.device != nullptr) {
        set.device->Release();
        set.device = nullptr;
    }

    set.logical_hash = 0u;
}

void release_cache() noexcept
{
    std::unordered_map<
        std::uint64_t,
        companion_set> dead;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        dead.swap(g_cache);
    }

    for (auto &entry : dead)
        release_set(entry.second);
}

void release_cache_for_device(
    ID3D11Device *device) noexcept
{
    if (device == nullptr)
        return;

    std::vector<companion_set> dead;

    {
        std::lock_guard<std::mutex> lock(g_mutex);

        for (auto it = g_cache.begin();
             it != g_cache.end();) {
            if (it->second.device == device) {
                dead.push_back(it->second);
                it = g_cache.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (auto &set : dead)
        release_set(set);
}

std::filesystem::path process_dir()
{
    std::wstring buffer(32768, L'\0');

    const DWORD n =
        GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size()));

    if (n == 0u ||
        n >= buffer.size())
        return {};

    buffer.resize(n);
    return std::filesystem::path(
        buffer).parent_path();
}

const wchar_t *class_dir(
    asset_class cls) noexcept
{
    switch (cls) {
    case asset_class::specular:
        return L"Specular";
    case asset_class::diffuse:
        return L"Diffuse";
    case asset_class::normal:
        return L"Normals";
    }

    return L"";
}

std::filesystem::path sidecar_path(
    asset_class cls,
    const std::wstring &logical_name)
{
    auto root = process_dir();
    if (root.empty() ||
        logical_name.empty())
        return {};

    std::filesystem::path leaf(
        logical_name);
    std::wstring filename =
        leaf.filename().wstring();

    if (filename.size() < 4u ||
        _wcsicmp(
            filename.c_str() +
                filename.size() - 4u,
            L".dds") != 0)
        filename += L".dds";

    return root /
        L"DSRRL" /
        class_dir(cls) /
        filename;
}

template <typename T>
bool read_struct(
    const std::vector<std::uint8_t> &bytes,
    std::size_t offset,
    T &out) noexcept
{
    if (offset > bytes.size() ||
        sizeof(T) >
            bytes.size() - offset)
        return false;

    std::memcpy(
        &out,
        bytes.data() + offset,
        sizeof(T));
    return true;
}

load_result load_dds(
    ID3D11Device *device,
    const std::filesystem::path &path) noexcept
{
    if (device == nullptr ||
        path.empty())
        return {};

    try {
        if (!std::filesystem::is_regular_file(path))
            return {};

        std::ifstream stream(
            path,
            std::ios::binary);
        if (!stream)
            return {};

        stream.seekg(
            0,
            std::ios::end);
        const auto end =
            stream.tellg();
        if (end <= 0)
            return {
                nullptr,
                load_status::unsupported
            };

        const auto size =
            static_cast<std::size_t>(end);

        stream.seekg(
            0,
            std::ios::beg);

        std::vector<std::uint8_t> bytes(size);
        if (!stream.read(
                reinterpret_cast<char *>(
                    bytes.data()),
                static_cast<std::streamsize>(
                    bytes.size())))
            return {
                nullptr,
                load_status::unsupported
            };

        std::uint32_t magic = 0u;
        dds_header header{};

        if (!read_struct(
                bytes,
                0u,
                magic) ||
            magic != k_dds_magic ||
            !read_struct(
                bytes,
                4u,
                header) ||
            header.size != 124u ||
            header.pixel_format.size != 32u ||
            header.width == 0u ||
            header.height == 0u)
            return {
                nullptr,
                load_status::unsupported
            };

        DXGI_FORMAT format =
            DXGI_FORMAT_UNKNOWN;
        std::size_t data_offset =
            4u + sizeof(dds_header);

        if (header.pixel_format.fourcc ==
            k_fourcc_dxt1) {
            format = DXGI_FORMAT_BC1_UNORM;
        } else if (
            header.pixel_format.fourcc ==
            k_fourcc_dxt3) {
            format = DXGI_FORMAT_BC2_UNORM;
        } else if (
            header.pixel_format.fourcc ==
            k_fourcc_dxt5) {
            format = DXGI_FORMAT_BC3_UNORM;
        } else if (
            header.pixel_format.fourcc ==
            k_fourcc_dx10) {
            dds_header_dx10 dx10{};

            if (!read_struct(
                    bytes,
                    data_offset,
                    dx10))
                return {
                    nullptr,
                    load_status::unsupported
                };

            data_offset +=
                sizeof(dx10);

            if (dx10.resource_dimension !=
                    k_resource_dimension_texture2d ||
                dx10.array_size != 1u ||
                (dx10.misc_flag &
                    k_misc_texturecube) != 0u)
                return {
                    nullptr,
                    load_status::unsupported
                };

            switch (
                static_cast<DXGI_FORMAT>(
                    dx10.dxgi_format)) {
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC7_UNORM:
                format =
                    static_cast<DXGI_FORMAT>(
                        dx10.dxgi_format);
                break;
            default:
                return {
                    nullptr,
                    load_status::unsupported
                };
            }
        } else {
            return {
                nullptr,
                load_status::unsupported
            };
        }

        if ((header.caps2 &
             k_caps2_cubemap) != 0u ||
            header.width > 16384u ||
            header.height > 16384u ||
            header.depth > 1u)
            return {
                nullptr,
                load_status::unsupported
            };

        const std::uint32_t mip_count =
            std::max(
                1u,
                header.mip_count);

        std::uint32_t max_mips = 1u;
        for (auto extent =
                 std::max(
                     header.width,
                     header.height);
             extent > 1u;
             extent >>= 1u)
            ++max_mips;

        if (mip_count > max_mips)
            return {
                nullptr,
                load_status::unsupported
            };

        const std::uint32_t block_bytes =
            format == DXGI_FORMAT_BC1_UNORM
                ? 8u
                : 16u;

        std::vector<
            D3D11_SUBRESOURCE_DATA> initial;
        initial.reserve(mip_count);

        std::size_t cursor =
            data_offset;
        std::uint32_t width =
            header.width;
        std::uint32_t height =
            header.height;

        for (std::uint32_t mip = 0u;
             mip < mip_count;
             ++mip) {
            const std::uint32_t blocks_w =
                std::max(
                    1u,
                    (width + 3u) / 4u);
            const std::uint32_t blocks_h =
                std::max(
                    1u,
                    (height + 3u) / 4u);

            const std::uint32_t row_pitch =
                blocks_w * block_bytes;
            const std::uint32_t slice_pitch =
                row_pitch * blocks_h;

            if (cursor > bytes.size() ||
                slice_pitch >
                    bytes.size() - cursor)
                return {
                    nullptr,
                    load_status::unsupported
                };

            D3D11_SUBRESOURCE_DATA sub{};
            sub.pSysMem =
                bytes.data() + cursor;
            sub.SysMemPitch =
                row_pitch;
            sub.SysMemSlicePitch =
                slice_pitch;
            initial.push_back(sub);

            cursor += slice_pitch;
            width =
                std::max(
                    1u,
                    width >> 1u);
            height =
                std::max(
                    1u,
                    height >> 1u);
        }

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = header.width;
        desc.Height = header.height;
        desc.MipLevels = mip_count;
        desc.ArraySize = 1u;
        desc.Format = format;
        desc.SampleDesc.Count = 1u;
        desc.Usage =
            D3D11_USAGE_IMMUTABLE;
        desc.BindFlags =
            D3D11_BIND_SHADER_RESOURCE;

        ID3D11Texture2D *texture =
            nullptr;
        ID3D11ShaderResourceView *view =
            nullptr;

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
            return {
                nullptr,
                load_status::create_failed
            };
        }

        return {
            view,
            load_status::ready
        };
    } catch (...) {
        g_internal_create = false;
        return {
            nullptr,
            load_status::create_failed
        };
    }
}

void account_load(
    load_status status) noexcept
{
    if (status == load_status::ready) {
        ++g_sidecar_ready;
    } else if (
        status == load_status::missing) {
        ++g_sidecar_missing;
    } else {
        ++g_sidecar_unsupported;
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
            reinterpret_cast<std::uintptr_t>(
                stock));

    std::lock_guard<std::mutex> lock(
        g_mutex);

    const auto found =
        g_cache.find(key);
    if (found == g_cache.end())
        return nullptr;

    ID3D11ShaderResourceView *view =
        nullptr;

    switch (cls) {
    case asset_class::specular:
        view = found->second.specular;
        break;
    case asset_class::diffuse:
        view = found->second.diffuse;
        break;
    case asset_class::normal:
        view = found->second.normal;
        break;
    }

    if (view != nullptr)
        view->AddRef();

    return view;
}

std::uint64_t logical_hash_for(
    ID3D11ShaderResourceView *stock) noexcept
{
    if (stock == nullptr)
        return 0u;

    const auto key =
        static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(
                stock));

    std::lock_guard<std::mutex> lock(
        g_mutex);

    const auto found =
        g_cache.find(key);

    return found == g_cache.end()
        ? 0u
        : found->second.logical_hash;
}

void on_init_resource_view(
    reshade::api::device *device,
    reshade::api::resource,
    reshade::api::resource_usage usage,
    const reshade::api::resource_view_desc &,
    reshade::api::resource_view view)
{
    if (g_internal_create ||
        g_quarantined.load() ||
        device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11 ||
        usage !=
            reshade::api::resource_usage::shader_resource ||
        view.handle == 0u)
        return;

    std::wstring logical_name;
    if (!texture_identity_transport::snapshot(
            logical_name))
        return;

    const auto logical_hash =
        fnv_name(logical_name);

    const bool spec_member =
        generated::spec_name_hash_allowed_v12(
            logical_hash);
    const bool diffuse_member =
        generated::diffuse_name_hash_allowed_v12(
            logical_hash);
    const bool normal_member =
        generated::normal_name_hash_allowed_v12(
            logical_hash);

    if (!spec_member &&
        !diffuse_member &&
        !normal_member)
        return;

    auto *native_device =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());

    if (native_device == nullptr)
        return;

    companion_set set{};
    set.device = native_device;
    set.device->AddRef();
    set.logical_hash =
        logical_hash;

    if (spec_member) {
        const auto loaded =
            load_dds(
                native_device,
                sidecar_path(
                    asset_class::specular,
                    logical_name));
        account_load(loaded.status);
        set.specular = loaded.view;
    }

    if (generated::
        diffuse_target_hash_allowed_v12(
            logical_hash)) {
        const auto loaded =
            load_dds(
                native_device,
                sidecar_path(
                    asset_class::diffuse,
                    logical_name));
        account_load(loaded.status);
        set.diffuse = loaded.view;
    }

    if (generated::
        normal_target_hash_allowed_v12(
            logical_hash)) {
        const auto loaded =
            load_dds(
                native_device,
                sidecar_path(
                    asset_class::normal,
                    logical_name));
        account_load(loaded.status);
        set.normal = loaded.view;
    }

    const auto key =
        static_cast<std::uint64_t>(
            view.handle);

    companion_set old{};
    bool had_old = false;

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);

        const auto found =
            g_cache.find(key);

        if (found != g_cache.end()) {
            old = found->second;
            found->second = set;
            had_old = true;
        } else {
            g_cache.emplace(
                key,
                set);
        }
    }

    set = {};

    if (had_old)
        release_set(old);

    ++g_named_views;
}

void on_destroy_resource_view(
    reshade::api::device *,
    reshade::api::resource_view view)
{
    companion_set dead{};
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);

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

void on_destroy_device(
    reshade::api::device *device)
{
    if (device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11)
        return;

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());

    release_cache_for_device(
        native);
}

bool append_retained_request(
    prepared_material_resource_draw &prepared,
    const island_draw_adapter_request &request,
    ID3D11ShaderResourceView *view) noexcept
{
    if (view == nullptr ||
        prepared.request_count >=
            prepared.requests.size() ||
        prepared.retained_count >=
            prepared.retained_views.size())
        return false;

    draw_tx_mutation verify{};
    if (build_island_draw_mutation(
            request,
            verify) !=
        island_draw_adapter_result::ready)
        return false;

    prepared.requests[
        prepared.request_count++] =
        request;
    prepared.retained_views[
        prepared.retained_count++] =
        view;
    return true;
}

} // namespace

material_resource_draw_runtime::
material_resource_draw_runtime(
    core::renderer_core &core) noexcept
    : core_(core)
{
}

material_resource_draw_runtime::
~material_resource_draw_runtime()
{
    unregister_events();
}

bool material_resource_draw_runtime::
register_events() noexcept
{
    if (g_core != nullptr &&
        g_core != &core_)
        return false;

    g_core = &core_;
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

    return true;
}

void material_resource_draw_runtime::
unregister_events() noexcept
{
    if (g_core != &core_)
        return;

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
    g_core = nullptr;
}

bool material_resource_draw_runtime::
prepare_draw_requests(
    ID3D11DeviceContext *context,
    std::uint32_t receiver_id,
    const operators::material_response::
        mtd_semantic_query &query,
    bool full_material_response_ready,
    prepared_material_resource_draw &prepared) noexcept
{
    prepared = {};

    if (context == nullptr ||
        receiver_id == 0u ||
        g_quarantined.load())
        return false;

    ID3D11ShaderResourceView *views[3]{};
    context->PSGetShaderResources(
        0u,
        3u,
        views);

    const auto h0 =
        logical_hash_for(views[0]);
    const auto h1 =
        logical_hash_for(views[1]);
    const auto h2 =
        logical_hash_for(views[2]);

    const bool exact_material =
        query.material.valid &&
        query.material.owner_tuple_exact;

    if (full_material_response_ready &&
        core_.features().enabled(
            core::operator_id::spec_rgb) &&
        receiver_id >= 24u &&
        receiver_id <= 47u) {
        auto *replacement =
            lookup(
                views[1],
                asset_class::specular);

        const bool exact_companion =
            h1 != 0u &&
            generated::
                spec_name_hash_allowed_v12(
                    h1) &&
            replacement != nullptr;

        operators::resource_bridges::
            spec_rgb_context context_spec{};
        context_spec.receiver_id =
            receiver_id;
        context_spec.actual_material_verified =
            exact_material;
        context_spec.material_specular_consumer_verified =
            operators::material_response::
                classify_mtd_semantic(
                    query,
                    operators::material_response::
                        mtd_semantic_operator::spec_rgb)
                .state ==
            operators::material_response::
                mtd_semantic_state::use;
        context_spec.exact_name_ptde_companion_verified =
            exact_companion;
        context_spec.ptde_sidecar_ready =
            replacement != nullptr;
        context_spec.native_t10_transport_ready =
            true;
        context_spec.stock_t1_preserved =
            true;

        const auto decision =
            operators::resource_bridges::
                evaluate_spec_rgb_route(
                    context_spec,
                    query);

        if (decision.action ==
                operators::resource_bridges::
                    spec_rgb_action::
                        bind_ptde_t10_rgb &&
            replacement != nullptr) {
            island_draw_adapter_request request{};
            request.primary =
                core::operator_id::spec_rgb;
            request.receiver_verified = true;
            request.material_verified = true;
            request.srvs[0] = {
                decision.ptde_rgb_srv_slot,
                replacement
            };
            request.srv_count = 1u;

            if (append_retained_request(
                    prepared,
                    request,
                    replacement)) {
                prepared.spec_rgb = true;
                ++g_spec_requests;
                replacement = nullptr;
            }
        }

        if (replacement != nullptr)
            replacement->Release();
    }

    const bool bmp_receiver =
        receiver_id >= 24u &&
        receiver_id <= 35u;

    const bool diffuse_pair =
        h0 != 0u &&
        h1 != 0u &&
        generated::diffuse_pair_allowed_v12(
            h1,
            h0);

    if (bmp_receiver &&
        full_material_response_ready &&
        core_.features().enabled(
            core::operator_id::diffuse)) {
        auto *replacement =
            diffuse_pair
                ? lookup(
                    views[0],
                    asset_class::diffuse)
                : nullptr;

        const auto semantic =
            operators::material_response::
                classify_mtd_semantic(
                    query,
                    operators::material_response::
                        mtd_semantic_operator::diffuse);

        operators::resource_bridges::
            diffuse_bridge_context context_diff{};
        context_diff.receiver_id =
            receiver_id;
        context_diff.actual_material_verified =
            exact_material &&
            semantic.state ==
                operators::material_response::
                    mtd_semantic_state::use;
        context_diff.actual_bound_t0_verified =
            h0 != 0u;
        context_diff.exact_ptde_diffuse_companion_verified =
            diffuse_pair &&
            replacement != nullptr;
        context_diff.ptde_srv_ready =
            replacement != nullptr;
        context_diff.ptde_c100_donor_verified =
            full_material_response_ready;
        context_diff.diffuse_linear_receiver_ready =
            full_material_response_ready;
        context_diff.shared_material_route =
            true;
        context_diff.exact_texture_identity_conjunction =
            diffuse_pair;

        const auto decision =
            operators::resource_bridges::
                evaluate_diffuse_route(
                    context_diff);

        if (decision.action ==
                operators::resource_bridges::
                    diffuse_action::
                        bind_ptde_t0_and_full_material_response &&
            replacement != nullptr) {
            island_draw_adapter_request request{};
            request.primary =
                core::operator_id::diffuse;
            request.receiver_verified = true;
            request.material_verified = true;
            request.srvs[0] = {
                decision.srv_slot,
                replacement
            };
            request.srv_count = 1u;

            if (append_retained_request(
                    prepared,
                    request,
                    replacement)) {
                prepared.diffuse = true;
                ++g_diffuse_requests;
                replacement = nullptr;
            }
        }

        if (replacement != nullptr)
            replacement->Release();
    }

    const bool normal_tuple =
        h0 != 0u &&
        h1 != 0u &&
        h2 != 0u &&
        generated::normal_tuple_allowed_v12(
            h0,
            h1,
            h2);

    if (bmp_receiver &&
        core_.features().enabled(
            core::operator_id::normal)) {
        auto *replacement =
            normal_tuple
                ? lookup(
                    views[2],
                    asset_class::normal)
                : nullptr;

        const auto semantic =
            operators::material_response::
                classify_mtd_semantic(
                    query,
                    operators::material_response::
                        mtd_semantic_operator::normal_bump);

        operators::resource_bridges::
            normal_bridge_context context_norm{};
        context_norm.receiver_id =
            receiver_id;

        if (semantic.state ==
            operators::material_response::
                mtd_semantic_state::use) {
            context_norm.authority =
                operators::resource_bridges::
                    normal_route_authority::
                        homologous_material_route;
            context_norm.homologous_bmp_material_route =
                exact_material;
        } else if (normal_tuple) {
            context_norm.authority =
                operators::resource_bridges::
                    normal_route_authority::
                        safe_exact_resource_tuple;
            context_norm.safe_exact_t0_t1_t2_tuple =
                true;
        }

        context_norm.target_unambiguous =
            normal_tuple;
        context_norm.actual_bound_t2_verified =
            h2 != 0u;
        context_norm.exact_ptde_normal_sidecar_verified =
            normal_tuple &&
            replacement != nullptr;
        context_norm.ptde_srv_ready =
            replacement != nullptr;
        context_norm.preserve_stock_s2 =
            true;

        const auto decision =
            operators::resource_bridges::
                evaluate_normal_route(
                    context_norm);

        if (decision.action ==
                operators::resource_bridges::
                    normal_action::bind_ptde_t2 &&
            replacement != nullptr) {
            island_draw_adapter_request request{};
            request.primary =
                core::operator_id::normal;
            request.receiver_verified = true;
            request.material_verified =
                exact_material ||
                normal_tuple;
            request.srvs[0] = {
                decision.srv_slot,
                replacement
            };
            request.srv_count = 1u;

            if (append_retained_request(
                    prepared,
                    request,
                    replacement)) {
                prepared.normal = true;
                ++g_normal_requests;
                replacement = nullptr;
            }
        }

        if (replacement != nullptr)
            replacement->Release();
    }

    for (auto *&view : views)
        release_view(view);

    if (prepared.request_count == 0u)
        ++g_fail_open;

    return true;
}

bool material_resource_draw_runtime::
prepare_subsurface_body_requests(
    ID3D11DeviceContext *context,
    std::uint32_t target_plain_receiver_id,
    prepared_material_resource_draw &prepared,
    operators::resource_bridges::subsurface_body_texture &body_texture) noexcept
{
    prepared = {};
    body_texture =
        operators::resource_bridges::
            subsurface_body_texture::unknown;

    if (context == nullptr ||
        g_quarantined.load() ||
        target_plain_receiver_id < 33u ||
        target_plain_receiver_id > 35u ||
        !core_.features().enabled(
            core::operator_id::spec_rgb) ||
        !core_.features().enabled(
            core::operator_id::diffuse) ||
        !core_.features().enabled(
            core::operator_id::normal))
        return false;

    ID3D11ShaderResourceView *views[3]{};
    context->PSGetShaderResources(
        0u,
        3u,
        views);

    const auto h0 =
        logical_hash_for(views[0]);
    const auto h1 =
        logical_hash_for(views[1]);
    const auto h2 =
        logical_hash_for(views[2]);

    constexpr std::uint64_t k_body_f_spec =
        0x724f5fe11b342205ull;
    constexpr std::uint64_t k_body_m_spec =
        0x777ede2aecc3d102ull;

    if (h1 == k_body_f_spec) {
        body_texture =
            operators::resource_bridges::
                subsurface_body_texture::bd_f_body_s;
    } else if (h1 == k_body_m_spec) {
        body_texture =
            operators::resource_bridges::
                subsurface_body_texture::bd_m_body_s;
    }

    const bool tuple_ready =
        body_texture !=
            operators::resource_bridges::
                subsurface_body_texture::unknown &&
        h0 != 0u &&
        h2 != 0u &&
        generated::spec_name_hash_allowed_v12(h1) &&
        generated::diffuse_pair_allowed_v12(
            h1,
            h0) &&
        generated::normal_tuple_allowed_v12(
            h0,
            h1,
            h2);

    ID3D11ShaderResourceView *spec = nullptr;
    ID3D11ShaderResourceView *diff = nullptr;
    ID3D11ShaderResourceView *norm = nullptr;

    if (tuple_ready) {
        spec = lookup(
            views[1],
            asset_class::specular);
        diff = lookup(
            views[0],
            asset_class::diffuse);
        norm = lookup(
            views[2],
            asset_class::normal);
    }

    for (auto *&view : views)
        release_view(view);

    if (spec == nullptr ||
        diff == nullptr ||
        norm == nullptr) {
        release_view(spec);
        release_view(diff);
        release_view(norm);
        prepared = {};
        return false;
    }

    island_draw_adapter_request spec_request{};
    spec_request.primary =
        core::operator_id::spec_rgb;
    spec_request.receiver_verified = true;
    spec_request.material_verified = true;
    spec_request.srvs[0] = {
        10u,
        spec
    };
    spec_request.srv_count = 1u;

    island_draw_adapter_request diff_request{};
    diff_request.primary =
        core::operator_id::diffuse;
    diff_request.receiver_verified = true;
    diff_request.material_verified = true;
    diff_request.srvs[0] = {
        0u,
        diff
    };
    diff_request.srv_count = 1u;

    island_draw_adapter_request norm_request{};
    norm_request.primary =
        core::operator_id::normal;
    norm_request.receiver_verified = true;
    norm_request.material_verified = true;
    norm_request.srvs[0] = {
        2u,
        norm
    };
    norm_request.srv_count = 1u;

    if (!append_retained_request(
            prepared,
            spec_request,
            spec)) {
        release_view(spec);
        release_view(diff);
        release_view(norm);
        prepared = {};
        return false;
    }
    spec = nullptr;

    if (!append_retained_request(
            prepared,
            diff_request,
            diff)) {
        release_view(diff);
        release_view(norm);
        release_prepared_draw(prepared);
        return false;
    }
    diff = nullptr;

    if (!append_retained_request(
            prepared,
            norm_request,
            norm)) {
        release_view(norm);
        release_prepared_draw(prepared);
        return false;
    }
    norm = nullptr;

    prepared.spec_rgb = true;
    prepared.diffuse = true;
    prepared.normal = true;
    ++g_spec_requests;
    ++g_diffuse_requests;
    ++g_normal_requests;
    return true;
}

void material_resource_draw_runtime::
release_prepared_draw(
    prepared_material_resource_draw &prepared) noexcept
{
    for (std::uint32_t i = 0u;
         i < prepared.retained_count;
         ++i) {
        if (prepared.retained_views[i] != nullptr)
            prepared.retained_views[i]->Release();
    }

    prepared = {};
}

material_resource_telemetry
material_resource_draw_runtime::
telemetry() const noexcept
{
    return {
        g_named_views.load(),
        g_sidecar_ready.load(),
        g_sidecar_missing.load(),
        g_sidecar_unsupported.load(),
        g_spec_requests.load(),
        g_diffuse_requests.load(),
        g_normal_requests.load(),
        g_fail_open.load(),
        g_quarantined.load()
    };
}

void material_resource_draw_runtime::
reset() noexcept
{
    release_cache();

    g_named_views.store(0u);
    g_sidecar_ready.store(0u);
    g_sidecar_missing.store(0u);
    g_sidecar_unsupported.store(0u);
    g_spec_requests.store(0u);
    g_diffuse_requests.store(0u);
    g_normal_requests.store(0u);
    g_fail_open.store(0u);
    g_quarantined.store(false);
}

} // namespace dsrrl::runtime
