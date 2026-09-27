#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/clustered_pnts_draw_runtime.hpp"

#include "dsrrl/operators/point_light/clustered_sidecar.hpp"
#include "dsrrl/runtime/flver_identity_transport.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"

#include <Windows.h>
#include <d3d11.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace dsrrl::runtime {
namespace {

using source_raw =
    operators::point_light::clustered_source_raw_v1;

struct producer_snapshot {
    std::uintptr_t owner = 0u;
    std::uint64_t serial = 0u;
    std::array<source_raw,4> sources{};
    std::uint8_t raw_selected_count = 0u;
    bool selector_mirror_verified = false;
    bool valid = false;
};

struct registry_entry {
    producer_snapshot snapshot{};
};

constexpr std::size_t k_registry_sets = 64u;
constexpr std::size_t k_registry_ways = 2u;
static_assert((k_registry_sets & (k_registry_sets - 1u)) == 0u);

std::array<
    std::array<registry_entry,k_registry_ways>,
    k_registry_sets> g_registry{};
std::mutex g_registry_mutex;

struct draw_selection_tls {
    producer_snapshot snapshot{};
    std::uint32_t material_max = 0u;
    bool owner_verified = false;
    bool material_limit_ready = false;
    bool ready = false;
};

thread_local draw_selection_tls g_draw_selection{};

struct gpu_resources {
    ID3D11Device *device = nullptr;
    ID3D11Buffer *t18_buffer = nullptr;
    ID3D11ShaderResourceView *t18_srv = nullptr;
    ID3D11Buffer *t19_buffer = nullptr;
    ID3D11ShaderResourceView *t19_srv = nullptr;
    ID3D11Buffer *b12 = nullptr;
};

std::mutex g_resource_mutex;
gpu_resources g_gpu{};

clustered_pnts_draw_runtime *g_runtime = nullptr;
std::atomic_bool g_enabled{false};
std::atomic_bool g_quarantined{false};
std::atomic<std::uint64_t> g_serial{0u};

std::atomic<std::uint64_t> g_builder_seen{0u};
std::atomic<std::uint64_t> g_collection_ok{0u};
std::atomic<std::uint64_t> g_collection_fail{0u};
std::atomic<std::uint64_t> g_selector_calls{0u};
std::atomic<std::uint64_t> g_mirror_equal{0u};
std::atomic<std::uint64_t> g_mirror_diff{0u};
std::atomic<std::uint64_t> g_source_capture_ok{0u};
std::atomic<std::uint64_t> g_source_capture_fail{0u};
std::atomic<std::uint64_t> g_snapshot_publish{0u};
std::atomic<std::uint64_t> g_selector_seen{0u};
std::atomic<std::uint64_t> g_owner_join_hit{0u};
std::atomic<std::uint64_t> g_owner_join_miss{0u};
std::atomic<std::uint64_t> g_material_limit_ok{0u};
std::atomic<std::uint64_t> g_material_limit_fail{0u};
std::atomic<std::uint64_t> g_sidecar_ready{0u};
std::atomic<std::uint64_t> g_sidecar_fail{0u};
std::atomic<std::uint64_t> g_t18_create{0u};
std::atomic<std::uint64_t> g_t18_hit{0u};
std::atomic<std::uint64_t> g_t19_create{0u};
std::atomic<std::uint64_t> g_t19_hit{0u};
std::atomic<std::uint64_t> g_b12_create{0u};
std::atomic<std::uint64_t> g_b12_hit{0u};
std::atomic<std::uint64_t> g_prepare_ok{0u};
std::atomic<std::uint64_t> g_prepare_fail{0u};

std::uintptr_t g_base = 0u;

using retained_selector_fn = int (__fastcall *)(
    void *,
    std::uint32_t *,
    int,
    const void *,
    std::uint8_t);

retained_selector_fn g_retained_selector = nullptr;

bool readable_range(
    const void *ptr,
    std::size_t size) noexcept
{
    if (ptr == nullptr)
        return false;
    if (size == 0u)
        return true;

    auto cursor =
        reinterpret_cast<std::uintptr_t>(ptr);
    const auto end = cursor + size;
    if (end < cursor)
        return false;

    while (cursor < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(
                reinterpret_cast<const void *>(cursor),
                &mbi,
                sizeof(mbi)) != sizeof(mbi))
            return false;

        if (mbi.State != MEM_COMMIT ||
            (mbi.Protect & PAGE_GUARD) != 0u)
            return false;

        const DWORD access = mbi.Protect & 0xffu;
        const bool readable =
            access == PAGE_READONLY ||
            access == PAGE_READWRITE ||
            access == PAGE_WRITECOPY ||
            access == PAGE_EXECUTE_READ ||
            access == PAGE_EXECUTE_READWRITE ||
            access == PAGE_EXECUTE_WRITECOPY;
        if (!readable)
            return false;

        const auto region_begin =
            reinterpret_cast<std::uintptr_t>(
                mbi.BaseAddress);
        const auto region_end =
            region_begin + mbi.RegionSize;
        if (region_end <= cursor ||
            region_end < region_begin)
            return false;

        cursor = std::min(region_end, end);
    }
    return true;
}

std::size_t registry_set(
    std::uintptr_t owner) noexcept
{
    const auto mixed =
        owner ^ (owner >> 17u) ^
        (owner >> 31u);
    return static_cast<std::size_t>(
        mixed & (k_registry_sets - 1u));
}

void publish_snapshot(
    const producer_snapshot &snapshot) noexcept
{
    const auto set = registry_set(snapshot.owner);
    std::lock_guard<std::mutex> lock(g_registry_mutex);

    auto *target = &g_registry[set][0];
    for (auto &way : g_registry[set]) {
        if (!way.snapshot.valid ||
            way.snapshot.owner == snapshot.owner) {
            target = &way;
            break;
        }
        if (way.snapshot.serial <
            target->snapshot.serial)
            target = &way;
    }

    target->snapshot = snapshot;
}

bool lookup_snapshot(
    std::uintptr_t owner,
    producer_snapshot &out) noexcept
{
    out = {};
    const auto set = registry_set(owner);
    std::lock_guard<std::mutex> lock(g_registry_mutex);

    const producer_snapshot *best = nullptr;
    for (const auto &way : g_registry[set]) {
        if (!way.snapshot.valid ||
            way.snapshot.owner != owner)
            continue;
        if (best == nullptr ||
            way.snapshot.serial > best->serial)
            best = &way.snapshot;
    }

    if (best == nullptr)
        return false;

    out = *best;
    return true;
}

bool spatial_overlap_xyz(
    const void *node,
    const float *query) noexcept
{
    if (!readable_range(node, 0x50u) ||
        !readable_range(query, 0x20u))
        return false;

    const auto *bytes =
        static_cast<const std::uint8_t *>(node);
    std::array<float,4> node_min{};
    std::array<float,4> node_max{};
    std::array<float,4> query_min{};
    std::array<float,4> query_max{};

    std::memcpy(
        node_min.data(),
        bytes + 0x30u,
        sizeof(node_min));
    std::memcpy(
        node_max.data(),
        bytes + 0x40u,
        sizeof(node_max));
    std::memcpy(
        query_min.data(),
        query,
        sizeof(query_min));
    std::memcpy(
        query_max.data(),
        query + 4,
        sizeof(query_max));

    for (std::size_t i = 0u; i < 3u; ++i) {
        if (node_max[i] < query_min[i] ||
            query_max[i] < node_min[i])
            return false;
    }
    return true;
}

bool mirror_first_four(
    void *collection,
    const float *query,
    std::uint8_t mask,
    std::array<std::uint32_t,4> &ids,
    std::array<void *,4> &nodes,
    std::uint8_t &count) noexcept
{
    ids = {};
    nodes = {};
    count = 0u;

    if (!readable_range(collection, 0x90u))
        return false;

    auto *base =
        static_cast<std::uint8_t *>(collection);

    for (std::uint32_t bucket = 0u;
         bucket < 4u && count < 4u;
         ++bucket) {
        if ((mask & (1u << bucket)) == 0u)
            continue;

        void *node = nullptr;
        std::memcpy(
            &node,
            base + 0x18u + bucket * 0x20u,
            sizeof(node));

        std::uint32_t guard = 0u;
        while (node != nullptr &&
               count < 4u) {
            if (++guard > 4096u ||
                !readable_range(node, 0x50u))
                return false;

            if (spatial_overlap_xyz(
                    node,
                    query)) {
                std::uint32_t id = 0u;
                std::memcpy(
                    &id,
                    static_cast<const std::uint8_t *>(
                        node) + 0x10u,
                    sizeof(id));

                ids[count] = id;
                nodes[count] = node;
                ++count;
                if (count == 4u)
                    break;
            }

            void *next = nullptr;
            std::memcpy(
                &next,
                static_cast<const std::uint8_t *>(
                    node) + 0x28u,
                sizeof(next));
            node = next;
        }
    }

    return true;
}

bool capture_source(
    void *node,
    source_raw &out) noexcept
{
    out = {};
    if (!readable_range(node, 0x20u))
        return false;

    void **vtable = nullptr;
    std::memcpy(&vtable, node, sizeof(vtable));
    if (!readable_range(
            vtable,
            13u * sizeof(void *)))
        return false;

    void *target = nullptr;
    std::memcpy(
        &target,
        vtable + 12u,
        sizeof(target));
    if (!readable_range(target, 1u))
        return false;

    using source_fn =
        void (__fastcall *)(void *, float *);
    const auto fn =
        reinterpret_cast<source_fn>(target);

    alignas(16) std::array<float,8> raw{};
    fn(node, raw.data());

    for (const auto value : raw)
        if (!std::isfinite(value))
            return false;

    if (!(raw[3] > 0.0f) ||
        !(raw[7] > 0.0f))
        return false;

    std::memcpy(
        &out.source_id,
        static_cast<const std::uint8_t *>(
            node) + 0x10u,
        sizeof(out.source_id));
    std::memcpy(
        &out.source_category,
        static_cast<const std::uint8_t *>(
            node) + 0x18u,
        sizeof(out.source_category));

    for (std::size_t i = 0u; i < 4u; ++i) {
        out.position_inv_range[i] = raw[i];
        out.raw_q_end[i] = raw[4u + i];
    }
    return true;
}

void release_gpu_locked() noexcept
{
    if (g_gpu.b12 != nullptr)
        g_gpu.b12->Release();
    if (g_gpu.t19_srv != nullptr)
        g_gpu.t19_srv->Release();
    if (g_gpu.t19_buffer != nullptr)
        g_gpu.t19_buffer->Release();
    if (g_gpu.t18_srv != nullptr)
        g_gpu.t18_srv->Release();
    if (g_gpu.t18_buffer != nullptr)
        g_gpu.t18_buffer->Release();
    if (g_gpu.device != nullptr)
        g_gpu.device->Release();
    g_gpu = {};
}

bool create_structured(
    ID3D11Device *device,
    UINT byte_width,
    UINT stride,
    ID3D11Buffer **buffer,
    ID3D11ShaderResourceView **srv) noexcept
{
    if (device == nullptr ||
        buffer == nullptr ||
        srv == nullptr)
        return false;

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = byte_width;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags =
        D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = stride;

    ID3D11Buffer *created = nullptr;
    if (FAILED(device->CreateBuffer(
            &desc,
            nullptr,
            &created)) ||
        created == nullptr)
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = DXGI_FORMAT_UNKNOWN;
    view.ViewDimension =
        D3D11_SRV_DIMENSION_BUFFER;
    view.Buffer.FirstElement = 0u;
    view.Buffer.NumElements =
        byte_width / stride;

    ID3D11ShaderResourceView *created_srv =
        nullptr;
    if (FAILED(device->CreateShaderResourceView(
            created,
            &view,
            &created_srv)) ||
        created_srv == nullptr) {
        created->Release();
        return false;
    }

    *buffer = created;
    *srv = created_srv;
    return true;
}

bool ensure_gpu(
    ID3D11DeviceContext *context) noexcept
{
    if (context == nullptr ||
        context->GetType() !=
            D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return false;

    ID3D11Device *device = nullptr;
    context->GetDevice(&device);
    if (device == nullptr)
        return false;

    std::lock_guard<std::mutex> lock(
        g_resource_mutex);

    if (g_gpu.device != nullptr &&
        g_gpu.device != device)
        release_gpu_locked();

    if (g_gpu.device == device &&
        g_gpu.t18_buffer != nullptr &&
        g_gpu.t18_srv != nullptr &&
        g_gpu.t19_buffer != nullptr &&
        g_gpu.t19_srv != nullptr &&
        g_gpu.b12 != nullptr) {
        telemetry::hot_count(g_t18_hit);
        telemetry::hot_count(g_t19_hit);
        telemetry::hot_count(g_b12_hit);
        device->Release();
        return true;
    }

    release_gpu_locked();

    if (!create_structured(
            device,
            static_cast<UINT>(
                sizeof(
                    operators::point_light::
                        clustered_t18_record_v1) *
                4u),
            static_cast<UINT>(
                sizeof(
                    operators::point_light::
                        clustered_t18_record_v1)),
            &g_gpu.t18_buffer,
            &g_gpu.t18_srv)) {
        device->Release();
        return false;
    }
    telemetry::hot_count(g_t18_create);

    if (!create_structured(
            device,
            static_cast<UINT>(
                sizeof(std::array<float,4>) *
                4u),
            static_cast<UINT>(
                sizeof(std::array<float,4>)),
            &g_gpu.t19_buffer,
            &g_gpu.t19_srv)) {
        release_gpu_locked();
        device->Release();
        return false;
    }
    telemetry::hot_count(g_t19_create);

    D3D11_BUFFER_DESC cb{};
    cb.ByteWidth = 64u;
    cb.Usage = D3D11_USAGE_DYNAMIC;
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device->CreateBuffer(
            &cb,
            nullptr,
            &g_gpu.b12)) ||
        g_gpu.b12 == nullptr) {
        release_gpu_locked();
        device->Release();
        return false;
    }
    telemetry::hot_count(g_b12_create);

    g_gpu.device = device;
    return true;
}

bool update_buffer(
    ID3D11DeviceContext *context,
    ID3D11Buffer *buffer,
    const void *data,
    std::size_t size) noexcept
{
    if (context == nullptr ||
        buffer == nullptr ||
        data == nullptr ||
        size == 0u)
        return false;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(
            buffer,
            0u,
            D3D11_MAP_WRITE_DISCARD,
            0u,
            &mapped)) ||
        mapped.pData == nullptr)
        return false;

    std::memcpy(mapped.pData, data, size);
    context->Unmap(buffer, 0u);
    return true;
}

void clear_registry() noexcept
{
    std::lock_guard<std::mutex> lock(
        g_registry_mutex);
    g_registry = {};
}

} // namespace

void clustered_pnts_builder_event_bridge(
    void *draw,
    void *renderer_context) noexcept
{
    if (g_runtime != nullptr)
        g_runtime->builder_event(
            draw,
            renderer_context);
}

void clustered_pnts_selector_event_bridge(
    void *owner,
    const void *actual_material) noexcept
{
    if (g_runtime != nullptr)
        g_runtime->selector_event(
            owner,
            actual_material);
}

bool clustered_pnts_draw_runtime::install() noexcept
{
    if (g_enabled.load())
        return g_runtime == this;
    if (g_runtime != nullptr &&
        g_runtime != this)
        return false;

    const auto flver =
        flver_identity_transport::status();
    if (!flver.provenance_ok ||
        !flver.selector_armed ||
        !flver.builder_armed)
        return false;

    g_base =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(nullptr));
    if (g_base == 0u)
        return false;

    g_retained_selector =
        reinterpret_cast<retained_selector_fn>(
            g_base + 0x55FC70u);

    g_runtime = this;
    g_quarantined.store(false);
    g_enabled.store(true);
    return true;
}

void clustered_pnts_draw_runtime::uninstall() noexcept
{
    g_enabled.store(false);
    consume_draw_selection();
    clear_registry();

    {
        std::lock_guard<std::mutex> lock(
            g_resource_mutex);
        release_gpu_locked();
    }

    g_retained_selector = nullptr;
    g_base = 0u;
    g_runtime = nullptr;
}

void clustered_pnts_draw_runtime::builder_event(
    void *draw,
    void *renderer_context) noexcept
{
    telemetry::hot_count(g_builder_seen);

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        draw == nullptr ||
        renderer_context == nullptr ||
        g_retained_selector == nullptr)
        return;

    if (!readable_range(
            renderer_context,
            0x2330u) ||
        !readable_range(
            draw,
            0xF0u)) {
        telemetry::hot_count(g_collection_fail);
        return;
    }

    void *collection = nullptr;
    std::memcpy(
        &collection,
        static_cast<const std::uint8_t *>(
            renderer_context) + 0x2328u,
        sizeof(collection));

    if (!readable_range(
            collection,
            0x90u)) {
        telemetry::hot_count(g_collection_fail);
        return;
    }
    telemetry::hot_count(g_collection_ok);

    auto *query =
        reinterpret_cast<const float *>(
            static_cast<const std::uint8_t *>(
                draw) + 0xD0u);

    std::uint8_t mask = 0u;
    std::memcpy(
        &mask,
        static_cast<const std::uint8_t *>(
            draw) + 0x3Au,
        sizeof(mask));

    std::array<std::uint32_t,4> host_ids{
        0xffffffffu,0xffffffffu,
        0xffffffffu,0xffffffffu};

    const int host_count =
        g_retained_selector(
            collection,
            host_ids.data(),
            4,
            query,
            mask);
    telemetry::hot_count(g_selector_calls);

    if (host_count < 0 ||
        host_count > 4)
        return;

    std::array<std::uint32_t,4> mirror_ids{};
    std::array<void *,4> nodes{};
    std::uint8_t mirror_count = 0u;
    if (!mirror_first_four(
            collection,
            query,
            mask,
            mirror_ids,
            nodes,
            mirror_count)) {
        telemetry::hot_count(g_mirror_diff);
        return;
    }

    if (static_cast<int>(mirror_count) !=
            host_count) {
        telemetry::hot_count(g_mirror_diff);
        return;
    }

    for (std::uint8_t i = 0u;
         i < mirror_count;
         ++i) {
        if (host_ids[i] != mirror_ids[i]) {
            telemetry::hot_count(g_mirror_diff);
            return;
        }
    }
    telemetry::hot_count(g_mirror_equal);

    if (mirror_count == 0u)
        return;

    producer_snapshot snapshot{};
    snapshot.owner =
        reinterpret_cast<std::uintptr_t>(
            renderer_context);
    snapshot.serial =
        g_serial.fetch_add(
            1u,
            std::memory_order_relaxed) + 1u;
    snapshot.raw_selected_count =
        mirror_count;
    snapshot.selector_mirror_verified =
        true;

    for (std::uint8_t i = 0u;
         i < mirror_count;
         ++i) {
        if (!capture_source(
                nodes[i],
                snapshot.sources[i])) {
            telemetry::hot_count(
                g_source_capture_fail);
            return;
        }

        if (snapshot.sources[i].source_id !=
            mirror_ids[i]) {
            telemetry::hot_count(
                g_source_capture_fail);
            return;
        }
        telemetry::hot_count(
            g_source_capture_ok);
    }

    snapshot.valid = true;
    publish_snapshot(snapshot);
    telemetry::hot_count(g_snapshot_publish);
}

void clustered_pnts_draw_runtime::selector_event(
    void *owner,
    const void *actual_material) noexcept
{
    telemetry::hot_count(g_selector_seen);
    g_draw_selection = {};

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        owner == nullptr ||
        actual_material == nullptr)
        return;

    producer_snapshot snapshot{};
    if (!lookup_snapshot(
            reinterpret_cast<std::uintptr_t>(
                owner),
            snapshot) ||
        !snapshot.valid ||
        !snapshot.selector_mirror_verified) {
        telemetry::hot_count(g_owner_join_miss);
        return;
    }
    telemetry::hot_count(g_owner_join_hit);

    if (!readable_range(
            actual_material,
            0x388u)) {
        telemetry::hot_count(
            g_material_limit_fail);
        return;
    }

    std::uint32_t material_max = 0u;
    std::memcpy(
        &material_max,
        static_cast<const std::uint8_t *>(
            actual_material) + 0x384u,
        sizeof(material_max));

    if (material_max == 0u) {
        telemetry::hot_count(
            g_material_limit_fail);
        return;
    }
    telemetry::hot_count(g_material_limit_ok);

    g_draw_selection.snapshot = snapshot;
    g_draw_selection.material_max = material_max;
    g_draw_selection.owner_verified = true;
    g_draw_selection.material_limit_ready =
        true;
    g_draw_selection.ready = true;
}

bool clustered_pnts_draw_runtime::prepare_sidecar(
    ID3D11DeviceContext *context,
    const operators::material_response::decision &material,
    prepared_clustered_pnts_draw &prepared) noexcept
{
    prepared = {};

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        context == nullptr ||
        !g_draw_selection.ready ||
        !g_draw_selection.owner_verified ||
        !g_draw_selection.material_limit_ready ||
        !material.active) {
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    const auto built =
        operators::point_light::
            build_clustered_sidecar_v1(
                g_draw_selection.snapshot.sources,
                g_draw_selection.snapshot.
                    raw_selected_count,
                g_draw_selection.material_max,
                material);

    if (built.result !=
            operators::point_light::
                clustered_sidecar_result_v1::ready ||
        !built.payload.ready) {
        telemetry::hot_count(g_sidecar_fail);
        telemetry::hot_count(g_prepare_fail);
        return false;
    }
    telemetry::hot_count(g_sidecar_ready);

    if (!ensure_gpu(context)) {
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    std::lock_guard<std::mutex> lock(
        g_resource_mutex);

    if (g_gpu.t18_buffer == nullptr ||
        g_gpu.t18_srv == nullptr ||
        g_gpu.t19_buffer == nullptr ||
        g_gpu.t19_srv == nullptr ||
        g_gpu.b12 == nullptr) {
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    if (!update_buffer(
            context,
            g_gpu.t18_buffer,
            built.payload.t18.data(),
            sizeof(built.payload.t18)) ||
        !update_buffer(
            context,
            g_gpu.t19_buffer,
            built.payload.t19.data(),
            sizeof(built.payload.t19)) ||
        !update_buffer(
            context,
            g_gpu.b12,
            built.payload.b12.data(),
            sizeof(built.payload.b12))) {
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    g_gpu.t18_srv->AddRef();
    g_gpu.t19_srv->AddRef();
    g_gpu.b12->AddRef();

    prepared.t18 = g_gpu.t18_srv;
    prepared.t19 = g_gpu.t19_srv;
    prepared.b12 = g_gpu.b12;
    prepared.producer_serial =
        g_draw_selection.snapshot.serial;
    prepared.raw_selected_count =
        built.payload.raw_selected_count;
    prepared.material_max_pnt_lit_num =
        built.payload.material_max_pnt_lit_num;
    prepared.effective_count =
        built.payload.effective_count;
    prepared.owner_verified = true;
    prepared.selector_mirror_verified =
        g_draw_selection.snapshot.
            selector_mirror_verified;
    prepared.ready = true;

    telemetry::hot_count(g_prepare_ok);
    return true;
}

void clustered_pnts_draw_runtime::release_prepared_draw(
    prepared_clustered_pnts_draw &prepared) noexcept
{
    if (prepared.b12 != nullptr)
        prepared.b12->Release();
    if (prepared.t19 != nullptr)
        prepared.t19->Release();
    if (prepared.t18 != nullptr)
        prepared.t18->Release();
    prepared = {};
}

void clustered_pnts_draw_runtime::consume_draw_selection() noexcept
{
    g_draw_selection = {};
}

void clustered_pnts_draw_runtime::on_destroy_device(
    reshade::api::device *device) noexcept
{
    if (device == nullptr)
        return;

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());

    std::lock_guard<std::mutex> lock(
        g_resource_mutex);

    if (g_gpu.device == native)
        release_gpu_locked();
}

clustered_pnts_telemetry
clustered_pnts_draw_runtime::telemetry() const noexcept
{
    return {
        g_builder_seen.load(),
        g_collection_ok.load(),
        g_collection_fail.load(),
        g_selector_calls.load(),
        g_mirror_equal.load(),
        g_mirror_diff.load(),
        g_source_capture_ok.load(),
        g_source_capture_fail.load(),
        g_snapshot_publish.load(),
        g_selector_seen.load(),
        g_owner_join_hit.load(),
        g_owner_join_miss.load(),
        g_material_limit_ok.load(),
        g_material_limit_fail.load(),
        g_sidecar_ready.load(),
        g_sidecar_fail.load(),
        g_t18_create.load(),
        g_t18_hit.load(),
        g_t19_create.load(),
        g_t19_hit.load(),
        g_b12_create.load(),
        g_b12_hit.load(),
        g_prepare_ok.load(),
        g_prepare_fail.load(),
        g_enabled.load(),
        g_quarantined.load()
    };
}

void clustered_pnts_draw_runtime::reset() noexcept
{
    consume_draw_selection();
    clear_registry();

    {
        std::lock_guard<std::mutex> lock(
            g_resource_mutex);
        release_gpu_locked();
    }

    g_serial.store(0u);
    g_builder_seen.store(0u);
    g_collection_ok.store(0u);
    g_collection_fail.store(0u);
    g_selector_calls.store(0u);
    g_mirror_equal.store(0u);
    g_mirror_diff.store(0u);
    g_source_capture_ok.store(0u);
    g_source_capture_fail.store(0u);
    g_snapshot_publish.store(0u);
    g_selector_seen.store(0u);
    g_owner_join_hit.store(0u);
    g_owner_join_miss.store(0u);
    g_material_limit_ok.store(0u);
    g_material_limit_fail.store(0u);
    g_sidecar_ready.store(0u);
    g_sidecar_fail.store(0u);
    g_t18_create.store(0u);
    g_t18_hit.store(0u);
    g_t19_create.store(0u);
    g_t19_hit.store(0u);
    g_b12_create.store(0u);
    g_b12_hit.store(0u);
    g_prepare_ok.store(0u);
    g_prepare_fail.store(0u);
    g_quarantined.store(false);
}

} // namespace dsrrl::runtime
