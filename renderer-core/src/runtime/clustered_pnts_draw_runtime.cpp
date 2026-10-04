#include "dsrrl/runtime/pointlight_ptde_source_runtime.hpp"
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
#include <unordered_map>

namespace dsrrl::runtime {
namespace {

using source_raw =
    operators::point_light::clustered_source_raw_v1;
using sidecar_payload =
    operators::point_light::clustered_sidecar_payload_v1;

struct producer_input_snapshot {
    std::uintptr_t owner = 0u;
    std::uint64_t serial = 0u;
    void *collection = nullptr;
    std::array<float,8> query{};
    std::uint8_t mask = 0u;
    bool valid = false;
};

struct draw_selection_tls {
    producer_input_snapshot input{};
    std::uint32_t material_max = 0u;
    operators::material_response::material_identity material{};
    operators::material_response::decision material_decision{};
    sidecar_payload payload{};
    clustered_pnts_prepare_failure cached_failure =
        clustered_pnts_prepare_failure::none;
    std::uint8_t sidecar_result_code = 0u;
    bool owner_verified = false;
    bool material_limit_ready = false;
    bool material_spc = false;
    bool authority_ready = false;
    bool payload_ready = false;
    bool neutral_no_pointlights = false;
    bool ready = false;
};

struct source_selection_cache_tls {
    std::uint64_t producer_serial = 0u;
    std::array<source_raw,4> sources{};
    std::uint8_t selected_count = 0u;
    clustered_pnts_prepare_failure failure =
        clustered_pnts_prepare_failure::none;
    bool attempted = false;
    bool ready = false;
    bool neutral = false;
};

thread_local draw_selection_tls g_draw_selection{};
thread_local source_selection_cache_tls g_source_selection_cache{};
thread_local producer_input_snapshot g_producer_input_tls{};
thread_local std::uint64_t g_local_serial = 0u;

struct gpu_resources {
    ID3D11Device *device = nullptr;
    ID3D11Buffer *t18_buffer = nullptr;
    ID3D11ShaderResourceView *t18_srv = nullptr;
    ID3D11Buffer *t19_buffer = nullptr;
    ID3D11ShaderResourceView *t19_srv = nullptr;
    ID3D11Buffer *b12 = nullptr;
    std::array<operators::point_light::clustered_t18_record_v1,4>
        last_t18{};
    std::array<std::array<float,4>,4> last_t19{};
    operators::material_response::material_response_b12_payload
        last_b12{};
    bool last_t18_valid = false;
    bool last_t19_valid = false;
    bool last_b12_valid = false;
};

std::mutex g_resource_mutex;
// Dynamic DISCARD payloads are recording-context local. A shared resource
// would let a second immediate/deferred context replace t18/t19/b12 between
// prepare and bind for the first context.
std::unordered_map<ID3D11DeviceContext *,gpu_resources>
    g_gpu_by_context{};

clustered_pnts_draw_runtime *g_runtime = nullptr;
std::atomic_bool g_enabled{false};
std::atomic_bool g_quarantined{false};
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
std::atomic<std::uint64_t> g_prepare_neutral_empty{0u};
std::atomic<std::uint64_t> g_prepare_precondition_fail{0u};
std::atomic<std::uint64_t> g_selection_fail{0u};
std::atomic<std::uint64_t> g_selection_empty{0u};
std::atomic<std::uint64_t> g_sidecar_build_fail{0u};
std::atomic<std::uint64_t> g_context_immediate{0u};
std::atomic<std::uint64_t> g_context_deferred{0u};
std::atomic<std::uint64_t> g_context_other{0u};
std::atomic<std::uint64_t> g_gpu_prepare_fail{0u};
std::atomic<std::uint64_t> g_upload_fail{0u};

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

struct readable_region_cache {
    std::uintptr_t begin = 0u;
    std::uintptr_t end = 0u;
};

bool readable_range_cached(
    const void *ptr,
    std::size_t size,
    readable_region_cache &cache) noexcept
{
    if (ptr == nullptr)
        return false;
    if (size == 0u)
        return true;

    const auto begin =
        reinterpret_cast<std::uintptr_t>(ptr);
    const auto end = begin + size;
    if (end < begin)
        return false;

    if (cache.begin <= begin &&
        end <= cache.end)
        return true;

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(
            ptr,
            &mbi,
            sizeof(mbi)) != sizeof(mbi) ||
        mbi.State != MEM_COMMIT ||
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
    if (region_end <= begin ||
        region_end < region_begin)
        return false;

    // Cache is deliberately draw-local at the caller. If an object crosses a
    // VM region boundary, retain the original full validator instead of
    // widening authority.
    if (end > region_end)
        return readable_range(ptr, size);

    cache.begin = region_begin;
    cache.end = region_end;
    return true;
}

bool executable_address(
    const void *ptr) noexcept
{
    if (ptr == nullptr)
        return false;

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(
            ptr,
            &mbi,
            sizeof(mbi)) != sizeof(mbi) ||
        mbi.State != MEM_COMMIT ||
        (mbi.Protect & PAGE_GUARD) != 0u)
        return false;

    const DWORD access = mbi.Protect & 0xffu;
    return
        access == PAGE_EXECUTE ||
        access == PAGE_EXECUTE_READ ||
        access == PAGE_EXECUTE_READWRITE ||
        access == PAGE_EXECUTE_WRITECOPY;
}

bool spatial_overlap_xyz_unchecked(
    const void *node,
    const std::array<float,4> &query_min,
    const std::array<float,4> &query_max) noexcept
{
    const auto *bytes =
        static_cast<const std::uint8_t *>(node);
    std::array<float,4> node_min{};
    std::array<float,4> node_max{};

    std::memcpy(
        node_min.data(),
        bytes + 0x30u,
        sizeof(node_min));
    std::memcpy(
        node_max.data(),
        bytes + 0x40u,
        sizeof(node_max));

    // Retail selector 0x14055FC70 performs the overlap test as one
    // four-lane SIMD compare in both directions. Preserve all four lanes
    // exactly; the semantic meaning of lane 3 is irrelevant to the carrier.
    for (std::size_t i = 0u; i < 4u; ++i) {
        if (node_max[i] < query_min[i] ||
            query_max[i] < node_min[i])
            return false;
    }
    return true;
}

bool select_first_four_exact(
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

    if (!readable_range(collection, 0x90u) ||
        query == nullptr)
        return false;

    std::array<float,4> query_min{};
    std::array<float,4> query_max{};
    std::memcpy(
        query_min.data(),
        query,
        sizeof(query_min));
    std::memcpy(
        query_max.data(),
        query + 4,
        sizeof(query_max));

    auto *base =
        static_cast<std::uint8_t *>(collection);
    // Valid only for this selector pass. Nodes allocated in the same committed
    // VM region reuse one VirtualQuery result, but no region verdict survives
    // into the next draw.
    readable_region_cache node_region{};

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
            // Validate each list node once. The previous diagnostic path
            // redundantly VirtualQuery'd the same node again inside the
            // overlap helper, which scaled badly with dense light lists.
            if (++guard > 4096u ||
                !readable_range_cached(
                    node,
                    0x50u,
                    node_region))
                return false;

            if (spatial_overlap_xyz_unchecked(
                    node,
                    query_min,
                    query_max)) {
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

    // The PTDE donor bridge accepts exactly these two attested retail source
    // classes. Reject every other source before calling its host vfunc: the
    // previous ordering paid a virtual call (and then exact donor validation)
    // for nodes that were guaranteed to fail open immediately afterwards.
    const auto target_address =
        reinterpret_cast<std::uintptr_t>(target);
    if (target_address != g_base + 0x55BC00u &&
        target_address != g_base + 0x55D0B0u)
        return false;
    if (!executable_address(target))
        return false;

    using source_fn =
        void (__fastcall *)(void *, float *);
    const auto fn =
        reinterpret_cast<source_fn>(target);

    alignas(16) std::array<float,8> raw{};
    fn(node, raw.data());

    // The retail CPU packer is homologous PTDE<->DSR for ordinary
    // BankPointLightEntity/LerpBankPointLightEntity: position, invRange,
    // packed RGB*Intensity and End have the same representation. Prefer the
    // exact PTDE donor when its bank authority resolves, but do not discard
    // an otherwise attested ordinary light solely because donor-bank identity
    // is unavailable. The clustered replacement shader still owns the proven
    // PTDE attenuation correction (x^3 -> x); unresolved numeric row residuals
    // remain source-local and are not compensated by arbitrary gains.
    (void)pointlight_ptde_source::capture(
        node,
        g_base,
        raw);

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

void release_gpu(
    gpu_resources &gpu) noexcept
{
    if (gpu.b12 != nullptr)
        gpu.b12->Release();
    if (gpu.t19_srv != nullptr)
        gpu.t19_srv->Release();
    if (gpu.t19_buffer != nullptr)
        gpu.t19_buffer->Release();
    if (gpu.t18_srv != nullptr)
        gpu.t18_srv->Release();
    if (gpu.t18_buffer != nullptr)
        gpu.t18_buffer->Release();
    if (gpu.device != nullptr)
        gpu.device->Release();
    gpu = {};
}

void release_all_gpu_locked() noexcept
{
    for (auto &entry : g_gpu_by_context)
        release_gpu(entry.second);
    g_gpu_by_context.clear();
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

bool ensure_gpu_locked(
    ID3D11DeviceContext *context,
    ID3D11Device *device,
    gpu_resources *&out) noexcept
{
    out = nullptr;
    if (context == nullptr || device == nullptr)
        return false;

    decltype(g_gpu_by_context)::iterator entry;
    try {
        entry =
            g_gpu_by_context.try_emplace(
                context).first;
    } catch (...) {
        return false;
    }

    auto &gpu = entry->second;
    if (gpu.device != nullptr &&
        gpu.device != device)
        release_gpu(gpu);

    const bool complete =
        gpu.device == device &&
        gpu.t18_buffer != nullptr &&
        gpu.t18_srv != nullptr &&
        gpu.t19_buffer != nullptr &&
        gpu.t19_srv != nullptr &&
        gpu.b12 != nullptr;

    if (complete) {
        telemetry::hot_count(g_t18_hit);
        telemetry::hot_count(g_t19_hit);
        telemetry::hot_count(g_b12_hit);
        out = &gpu;
        return true;
    }

    // Never reuse a partial carrier after a failed creation attempt.
    release_gpu(gpu);

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
            &gpu.t18_buffer,
            &gpu.t18_srv))
        return false;
    telemetry::hot_count(g_t18_create);

    if (!create_structured(
            device,
            static_cast<UINT>(
                sizeof(std::array<float,4>) *
                4u),
            static_cast<UINT>(
                sizeof(std::array<float,4>)),
            &gpu.t19_buffer,
            &gpu.t19_srv)) {
        release_gpu(gpu);
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
            &gpu.b12)) ||
        gpu.b12 == nullptr) {
        release_gpu(gpu);
        return false;
    }
    telemetry::hot_count(g_b12_create);

    gpu.device = device;
    gpu.device->AddRef();
    out = &gpu;
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

void clustered_pnts_selector_identity_event_bridge(
    const operators::material_response::material_identity &identity,
    bool expected_spc) noexcept
{
    if (g_runtime != nullptr)
        g_runtime->selector_identity_event(
            identity,
            expected_spc);
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
    g_source_selection_cache = {};
    g_producer_input_tls = {};
    g_local_serial = 0u;

    {
        std::lock_guard<std::mutex> lock(
            g_resource_mutex);
        release_all_gpu_locked();
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

    // The 0x14022084F ordinary builder hook is extremely hot. Never traverse
    // PointLight lists, call source vfuncs, VirtualQuery draw memory or take a
    // global lock here. Capture only the exact selector inputs that the later
    // authorized draw may need.
    g_producer_input_tls = {};

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        draw == nullptr ||
        renderer_context == nullptr)
        return;

    producer_input_snapshot input{};
    input.owner =
        reinterpret_cast<std::uintptr_t>(
            renderer_context);
    // Producer snapshot and selector join are both thread-local and
    // explicitly fail open across threads. A process-wide atomic serial added
    // a locked RMW to this extremely hot builder for no semantic benefit.
    input.serial =
        ++g_local_serial;

    const auto *renderer_bytes =
        static_cast<const std::uint8_t *>(
            renderer_context);
    const auto *draw_bytes =
        static_cast<const std::uint8_t *>(
            draw);

    // These exact objects/offsets are already live at the attested retail
    // builder site. Snapshot only draw-local state; external collection/node
    // memory is validated later, and only for an authorized PointLight draw.
    std::memcpy(
        &input.collection,
        renderer_bytes + 0x2328u,
        sizeof(input.collection));
    std::memcpy(
        input.query.data(),
        draw_bytes + 0xD0u,
        sizeof(input.query));
    std::memcpy(
        &input.mask,
        draw_bytes + 0x3Au,
        sizeof(input.mask));

    if (input.collection == nullptr ||
        (input.mask & 0x0fu) == 0u) {
        telemetry::hot_count(g_collection_fail);
        return;
    }

    input.valid = true;
    g_producer_input_tls = input;
    telemetry::hot_count(g_collection_ok);
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

    const auto owner_key =
        reinterpret_cast<std::uintptr_t>(
            owner);

    // Exact same-thread handoff only. Cross-thread delivery intentionally
    // fails open instead of resurrecting the old globally locked registry.
    if (!g_producer_input_tls.valid ||
        g_producer_input_tls.owner !=
            owner_key) {
        telemetry::hot_count(g_owner_join_miss);
        return;
    }
    telemetry::hot_count(g_owner_join_hit);

    // Exact retail return-RVA validation and resolve_actual_material() have
    // already authenticated this fixed-layout material object. Match the
    // existing FLVER draw-hot policy: direct fixed-layout read, no
    // VirtualQuery/syscall on this multi-million-call path.
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

    g_draw_selection.input =
        g_producer_input_tls;
    g_draw_selection.material_max = material_max;
    g_draw_selection.owner_verified = true;
    g_draw_selection.material_limit_ready =
        true;
    g_draw_selection.ready = true;
}
void clustered_pnts_draw_runtime::selector_identity_event(
    const operators::material_response::material_identity &identity,
    bool expected_spc) noexcept
{
    if (!g_enabled.load() ||
        g_quarantined.load() ||
        !g_draw_selection.ready ||
        !g_draw_selection.owner_verified ||
        !g_draw_selection.material_limit_ready)
        return;

    const auto decision =
        operators::material_response::
            evaluate_direct_pointlight_material_identity(
                identity,
                expected_spc);

    if (!decision.active)
        return;

    g_draw_selection.material = identity;
    g_draw_selection.material_decision = decision;
    g_draw_selection.material_spc = expected_spc;
    g_draw_selection.authority_ready = true;

    const auto &input = g_draw_selection.input;
    auto &source_cache = g_source_selection_cache;

    if (source_cache.producer_serial != input.serial) {
        source_cache = {};
        source_cache.producer_serial = input.serial;
        source_cache.attempted = true;

        std::array<std::uint32_t,4> selected_ids{};
        std::array<void *,4> nodes{};
        std::uint8_t selected_count = 0u;

        if (!input.valid ||
            !select_first_four_exact(
                input.collection,
                input.query.data(),
                input.mask,
                selected_ids,
                nodes,
                selected_count)) {
            source_cache.failure =
                clustered_pnts_prepare_failure::selection;
            telemetry::hot_count(g_mirror_diff);
            telemetry::hot_count(g_selection_fail);
            telemetry::hot_count(g_sidecar_fail);
            return;
        }
        telemetry::hot_count(g_selector_calls);

#if defined(DSRRL_CLUSTERED_SELECTOR_RUNTIME_CROSSCHECK)
        if (g_retained_selector == nullptr) {
            source_cache.failure =
                clustered_pnts_prepare_failure::selection;
            telemetry::hot_count(g_mirror_diff);
            telemetry::hot_count(g_selection_fail);
            telemetry::hot_count(g_sidecar_fail);
            return;
        }

        std::array<std::uint32_t,4> host_ids{
            0xffffffffu,0xffffffffu,
            0xffffffffu,0xffffffffu};
        const int host_count =
            g_retained_selector(
                input.collection,
                host_ids.data(),
                4,
                input.query.data(),
                input.mask);

        if (host_count < 0 ||
            host_count > 4 ||
            static_cast<int>(selected_count) !=
                host_count) {
            source_cache.failure =
                clustered_pnts_prepare_failure::selection;
            telemetry::hot_count(g_mirror_diff);
            telemetry::hot_count(g_selection_fail);
            telemetry::hot_count(g_sidecar_fail);
            return;
        }

        for (std::uint8_t i = 0u;
             i < selected_count;
             ++i) {
            if (host_ids[i] != selected_ids[i]) {
                source_cache.failure =
                    clustered_pnts_prepare_failure::selection;
                telemetry::hot_count(g_mirror_diff);
                telemetry::hot_count(g_selection_fail);
                telemetry::hot_count(g_sidecar_fail);
                return;
            }
        }
        telemetry::hot_count(g_mirror_equal);
#endif

        source_cache.selected_count = selected_count;
        if (selected_count == 0u) {
            source_cache.ready = true;
            source_cache.neutral = true;
            telemetry::hot_count(g_selection_empty);
        } else {
            for (std::uint8_t i = 0u;
                 i < selected_count;
                 ++i) {
                if (!capture_source(
                        nodes[i],
                        source_cache.sources[i]) ||
                    source_cache.sources[i].source_id !=
                        selected_ids[i]) {
                    source_cache.failure =
                        clustered_pnts_prepare_failure::source_capture;
                    telemetry::hot_count(g_source_capture_fail);
                    telemetry::hot_count(g_sidecar_fail);
                    return;
                }
                telemetry::hot_count(g_source_capture_ok);
            }
            source_cache.ready = true;
        }
    }

    if (!source_cache.ready) {
        g_draw_selection.cached_failure =
            source_cache.failure;
        return;
    }

    if (source_cache.neutral) {
        g_draw_selection.neutral_no_pointlights = true;
        g_draw_selection.cached_failure =
            clustered_pnts_prepare_failure::empty_selection;
        telemetry::hot_count(g_prepare_neutral_empty);
        return;
    }

    const auto built =
        operators::point_light::
            build_clustered_sidecar_v1(
                source_cache.sources,
                source_cache.selected_count,
                g_draw_selection.material_max,
                decision);

    g_draw_selection.sidecar_result_code =
        static_cast<std::uint8_t>(built.result);

    if (built.result !=
            operators::point_light::
                clustered_sidecar_result_v1::ready ||
        !built.payload.ready) {
        g_draw_selection.cached_failure =
            clustered_pnts_prepare_failure::sidecar_build;
        telemetry::hot_count(g_sidecar_build_fail);
        telemetry::hot_count(g_sidecar_fail);
        return;
    }

    g_draw_selection.payload = built.payload;
    g_draw_selection.payload_ready = true;
    telemetry::hot_count(g_sidecar_ready);
}

bool clustered_pnts_draw_runtime::current_draw_authority(
    bool expected_spc,
    operators::material_response::material_identity &material,
    operators::material_response::decision &decision) const noexcept
{
    material = {};
    decision = {};

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        !g_draw_selection.ready ||
        !g_draw_selection.authority_ready ||
        g_draw_selection.material_spc != expected_spc)
        return false;

    material = g_draw_selection.material;
    decision = g_draw_selection.material_decision;
    return decision.active;
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
        prepared.failure =
            clustered_pnts_prepare_failure::precondition;
        telemetry::hot_count(g_prepare_precondition_fail);
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    const auto &input =
        g_draw_selection.input;

    if (!g_draw_selection.authority_ready ||
        g_draw_selection.material_decision.route_index !=
            material.route_index) {
        prepared.failure =
            clustered_pnts_prepare_failure::precondition;
        telemetry::hot_count(g_prepare_precondition_fail);
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    if (g_draw_selection.neutral_no_pointlights) {
        prepared.producer_serial = input.serial;
        prepared.raw_selected_count = 0u;
        prepared.material_max_pnt_lit_num =
            g_draw_selection.material_max;
        prepared.effective_count = 0u;
        prepared.owner_verified = true;
        prepared.selector_mirror_verified = true;
        prepared.neutral_no_pointlights = true;
        return false;
    }

    if (!g_draw_selection.payload_ready) {
        prepared.failure =
            g_draw_selection.cached_failure !=
                    clustered_pnts_prepare_failure::none
                ? g_draw_selection.cached_failure
                : clustered_pnts_prepare_failure::sidecar_build;
        prepared.sidecar_result_code =
            g_draw_selection.sidecar_result_code;
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    const auto &payload =
        g_draw_selection.payload;

    const auto context_type = context->GetType();
    if (context_type == D3D11_DEVICE_CONTEXT_IMMEDIATE)
        telemetry::hot_count(g_context_immediate);
    else if (context_type == D3D11_DEVICE_CONTEXT_DEFERRED)
        telemetry::hot_count(g_context_deferred);
    else
        telemetry::hot_count(g_context_other);

    ID3D11Device *device = nullptr;
    context->GetDevice(&device);
    if (device == nullptr) {
        prepared.failure =
            clustered_pnts_prepare_failure::gpu_prepare;
        telemetry::hot_count(g_gpu_prepare_fail);
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    // Resource lookup/creation is process-global, but the dynamic payloads
    // themselves are recording-context local. Hold the mutex only long enough
    // to resolve/create that context's carrier and retain the COM objects.
    // Map(WRITE_DISCARD)/Unmap must execute outside the global lock: otherwise
    // independent deferred contexts (notably reflective-water work) serialize
    // all three PointLight uploads through one mutex for no semantic reason.
    ID3D11Buffer *t18_buffer = nullptr;
    ID3D11ShaderResourceView *t18_srv = nullptr;
    ID3D11Buffer *t19_buffer = nullptr;
    ID3D11ShaderResourceView *t19_srv = nullptr;
    ID3D11Buffer *b12 = nullptr;
    bool upload_t18 = true;
    bool upload_t19 = true;
    bool upload_b12 = true;

    {
        std::lock_guard<std::mutex> lock(
            g_resource_mutex);

        gpu_resources *gpu = nullptr;
        if (!ensure_gpu_locked(
                context,
                device,
                gpu)) {
            device->Release();
            prepared.failure =
                clustered_pnts_prepare_failure::gpu_prepare;
            telemetry::hot_count(g_gpu_prepare_fail);
            telemetry::hot_count(g_prepare_fail);
            return false;
        }

        if (gpu == nullptr ||
            gpu->t18_buffer == nullptr ||
            gpu->t18_srv == nullptr ||
            gpu->t19_buffer == nullptr ||
            gpu->t19_srv == nullptr ||
            gpu->b12 == nullptr) {
            device->Release();
            prepared.failure =
                clustered_pnts_prepare_failure::gpu_resources;
            telemetry::hot_count(g_prepare_fail);
            return false;
        }

        t18_buffer = gpu->t18_buffer;
        t18_srv = gpu->t18_srv;
        t19_buffer = gpu->t19_buffer;
        t19_srv = gpu->t19_srv;
        b12 = gpu->b12;

        upload_t18 =
            !gpu->last_t18_valid ||
            std::memcmp(
                gpu->last_t18.data(),
                payload.t18.data(),
                sizeof(payload.t18)) != 0;
        upload_t19 =
            !gpu->last_t19_valid ||
            std::memcmp(
                gpu->last_t19.data(),
                payload.t19.data(),
                sizeof(payload.t19)) != 0;
        upload_b12 =
            !gpu->last_b12_valid ||
            std::memcmp(
                gpu->last_b12.data(),
                payload.b12.data(),
                sizeof(payload.b12)) != 0;

        // Keep the exact carrier alive after releasing g_resource_mutex. The
        // destroy-device path may remove the cache entry concurrently, but it
        // cannot destroy resources retained here until this prepare completes.
        t18_buffer->AddRef();
        t19_buffer->AddRef();
        t18_srv->AddRef();
        t19_srv->AddRef();
        b12->AddRef();
    }

    device->Release();

    const bool uploaded =
        (!upload_t18 ||
         update_buffer(
             context,
             t18_buffer,
             payload.t18.data(),
             sizeof(payload.t18))) &&
        (!upload_t19 ||
         update_buffer(
             context,
             t19_buffer,
             payload.t19.data(),
             sizeof(payload.t19))) &&
        (!upload_b12 ||
         update_buffer(
             context,
             b12,
             payload.b12.data(),
             sizeof(payload.b12)));

    if (uploaded &&
        (upload_t18 || upload_t19 || upload_b12)) {
        std::lock_guard<std::mutex> lock(
            g_resource_mutex);
        const auto found =
            g_gpu_by_context.find(context);
        if (found != g_gpu_by_context.end() &&
            found->second.t18_buffer == t18_buffer &&
            found->second.t19_buffer == t19_buffer &&
            found->second.b12 == b12) {
            auto &gpu = found->second;
            if (upload_t18) {
                gpu.last_t18 = payload.t18;
                gpu.last_t18_valid = true;
            }
            if (upload_t19) {
                gpu.last_t19 = payload.t19;
                gpu.last_t19_valid = true;
            }
            if (upload_b12) {
                gpu.last_b12 = payload.b12;
                gpu.last_b12_valid = true;
            }
        }
    }

    // Buffer refs are temporary upload-lifetime guards. SRV/b12 refs transfer
    // to prepared and are released by release_prepared_draw().
    t18_buffer->Release();
    t19_buffer->Release();

    if (!uploaded) {
        t18_srv->Release();
        t19_srv->Release();
        b12->Release();
        prepared.failure =
            clustered_pnts_prepare_failure::upload;
        telemetry::hot_count(g_upload_fail);
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    prepared.t18 = t18_srv;
    prepared.t19 = t19_srv;
    prepared.b12 = b12;
    prepared.producer_serial =
        input.serial;
    prepared.raw_selected_count =
        payload.raw_selected_count;
    prepared.material_max_pnt_lit_num =
        payload.material_max_pnt_lit_num;
    prepared.effective_count =
        payload.effective_count;
    prepared.owner_verified = true;
    prepared.selector_mirror_verified = true;
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

    for (auto it =
             g_gpu_by_context.begin();
         it != g_gpu_by_context.end();) {
        if (it->second.device == native) {
            release_gpu(it->second);
            it =
                g_gpu_by_context.erase(it);
        } else {
            ++it;
        }
    }
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
        g_prepare_neutral_empty.load(),
        g_prepare_precondition_fail.load(),
        g_selection_fail.load(),
        g_selection_empty.load(),
        g_sidecar_build_fail.load(),
        g_context_immediate.load(),
        g_context_deferred.load(),
        g_context_other.load(),
        g_gpu_prepare_fail.load(),
        g_upload_fail.load(),
        g_enabled.load(),
        g_quarantined.load()
    };
}

void clustered_pnts_draw_runtime::reset() noexcept
{
    consume_draw_selection();
    g_source_selection_cache = {};
    g_producer_input_tls = {};

    {
        std::lock_guard<std::mutex> lock(
            g_resource_mutex);
        release_all_gpu_locked();
    }

    g_local_serial = 0u;
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
    g_prepare_neutral_empty.store(0u);
    g_prepare_precondition_fail.store(0u);
    g_selection_fail.store(0u);
    g_selection_empty.store(0u);
    g_sidecar_build_fail.store(0u);
    g_context_immediate.store(0u);
    g_context_deferred.store(0u);
    g_context_other.store(0u);
    g_gpu_prepare_fail.store(0u);
    g_upload_fail.store(0u);
    g_quarantined.store(false);
}

} // namespace dsrrl::runtime
