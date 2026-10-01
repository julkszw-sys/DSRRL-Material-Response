#include "dsrrl/runtime/pmetal_native_draw_bridge.hpp"
#include "dsrrl/runtime/d3d11_cb_window.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dsrrl::runtime {
namespace {

constexpr std::size_t k_vtbl_draw_indexed = 12u;
constexpr std::size_t k_vtbl_draw = 13u;
constexpr std::size_t k_vtbl_draw_indexed_instanced = 20u;
constexpr std::size_t k_vtbl_draw_instanced = 21u;

using draw_fn =
    void (STDMETHODCALLTYPE *)(
        ID3D11DeviceContext *,
        UINT,
        UINT);
using draw_indexed_fn =
    void (STDMETHODCALLTYPE *)(
        ID3D11DeviceContext *,
        UINT,
        UINT,
        INT);
using draw_instanced_fn =
    void (STDMETHODCALLTYPE *)(
        ID3D11DeviceContext *,
        UINT,
        UINT,
        UINT,
        UINT);
using draw_indexed_instanced_fn =
    void (STDMETHODCALLTYPE *)(
        ID3D11DeviceContext *,
        UINT,
        UINT,
        UINT,
        INT,
        UINT);

enum class pending_kind : std::uint8_t {
    none = 0,
    draw,
    draw_indexed
};

struct pending_draw {
    pmetal_native_draw_bridge *owner = nullptr;
    ID3D11DeviceContext *context = nullptr;
    draw_tx_mutation mutation{};
    pending_kind kind = pending_kind::none;
    std::uint32_t count = 0u;
    std::uint32_t instance_count = 0u;
    std::uint32_t first = 0u;
    std::int32_t vertex_offset = 0;
    std::uint32_t first_instance = 0u;
    bool active = false;
};

thread_local pending_draw g_pending{};
std::atomic<pmetal_native_draw_bridge *> g_active{nullptr};

draw_fn g_original_draw = nullptr;
draw_indexed_fn g_original_draw_indexed = nullptr;
draw_instanced_fn g_original_draw_instanced = nullptr;
draw_indexed_instanced_fn g_original_draw_indexed_instanced = nullptr;

void retain_mutation(
    draw_tx_mutation &mutation) noexcept
{
    if (mutation.replace_pixel_shader &&
        mutation.pixel_shader != nullptr)
        mutation.pixel_shader->AddRef();

    for (std::uint32_t i = 0u;
         i < mutation.constant_buffer_count;
         ++i)
        if (mutation.constant_buffers[i].buffer != nullptr)
            mutation.constant_buffers[i].buffer->AddRef();

    for (std::uint32_t i = 0u;
         i < mutation.srv_count;
         ++i)
        if (mutation.srvs[i].srv != nullptr)
            mutation.srvs[i].srv->AddRef();

    for (std::uint32_t i = 0u;
         i < mutation.sampler_count;
         ++i)
        if (mutation.samplers[i].sampler != nullptr)
            mutation.samplers[i].sampler->AddRef();
}

void release_mutation(
    draw_tx_mutation &mutation) noexcept
{
    if (mutation.replace_pixel_shader &&
        mutation.pixel_shader != nullptr)
        mutation.pixel_shader->Release();

    for (std::uint32_t i = 0u;
         i < mutation.constant_buffer_count;
         ++i)
        if (mutation.constant_buffers[i].buffer != nullptr)
            mutation.constant_buffers[i].buffer->Release();

    for (std::uint32_t i = 0u;
         i < mutation.srv_count;
         ++i)
        if (mutation.srvs[i].srv != nullptr)
            mutation.srvs[i].srv->Release();

    for (std::uint32_t i = 0u;
         i < mutation.sampler_count;
         ++i)
        if (mutation.samplers[i].sampler != nullptr)
            mutation.samplers[i].sampler->Release();

    mutation = {};
}

struct cb_capture {
    std::uint32_t slot = 0u;
    ID3D11Buffer *base = nullptr;
    ID3D11Buffer *window = nullptr;
    UINT first = 0u;
    UINT count = 0u;
    bool explicit_window = false;
};

struct srv_capture {
    std::uint32_t slot = 0u;
    ID3D11ShaderResourceView *value = nullptr;
};

struct sampler_capture {
    std::uint32_t slot = 0u;
    ID3D11SamplerState *value = nullptr;
};

struct native_state {
    ID3D11PixelShader *shader = nullptr;
    std::array<ID3D11ClassInstance *,
               draw_tx_max_class_instances> classes{};
    UINT class_count = 0u;
    bool shader_captured = false;

    std::array<cb_capture,draw_tx_max_cb> cbs{};
    std::uint32_t cb_count = 0u;

    std::array<srv_capture,draw_tx_max_srv> srvs{};
    std::uint32_t srv_count = 0u;

    std::array<sampler_capture,draw_tx_max_sampler> samplers{};
    std::uint32_t sampler_count = 0u;
};

void release_native_state(
    native_state &state) noexcept
{
    if (state.shader != nullptr)
        state.shader->Release();

    for (std::uint32_t i = 0u;
         i < state.class_count &&
         i < state.classes.size();
         ++i)
        if (state.classes[i] != nullptr)
            state.classes[i]->Release();

    for (std::uint32_t i = 0u;
         i < state.cb_count;
         ++i) {
        if (state.cbs[i].base != nullptr)
            state.cbs[i].base->Release();
        if (state.cbs[i].window != nullptr)
            state.cbs[i].window->Release();
    }

    for (std::uint32_t i = 0u;
         i < state.srv_count;
         ++i)
        if (state.srvs[i].value != nullptr)
            state.srvs[i].value->Release();

    for (std::uint32_t i = 0u;
         i < state.sampler_count;
         ++i)
        if (state.samplers[i].value != nullptr)
            state.samplers[i].value->Release();

    state = {};
}

bool capture_state(
    ID3D11DeviceContext *context,
    ID3D11DeviceContext1 *context1,
    const draw_tx_mutation &mutation,
    native_state &state) noexcept
{
    state = {};

    if (context == nullptr ||
        mutation.constant_buffer_count > draw_tx_max_cb ||
        mutation.srv_count > draw_tx_max_srv ||
        mutation.sampler_count > draw_tx_max_sampler)
        return false;

    if (mutation.replace_pixel_shader) {
        state.class_count =
            static_cast<UINT>(
                state.classes.size());
        context->PSGetShader(
            &state.shader,
            state.classes.data(),
            &state.class_count);

        if (state.shader == nullptr ||
            state.class_count >
                state.classes.size()) {
            release_native_state(state);
            return false;
        }

        state.shader_captured = true;
    }

    state.cb_count =
        mutation.constant_buffer_count;
    for (std::uint32_t i = 0u;
         i < state.cb_count;
         ++i) {
        auto &capture = state.cbs[i];
        capture.slot =
            mutation.constant_buffers[i].slot;

        context->PSGetConstantBuffers(
            capture.slot,
            1u,
            &capture.base);

        if (context1 != nullptr) {
            context1->PSGetConstantBuffers1(
                capture.slot,
                1u,
                &capture.window,
                &capture.first,
                &capture.count);

            if (capture.base != capture.window) {
                release_native_state(state);
                return false;
            }

            capture.explicit_window =
                capture.window != nullptr &&
                capture.count >= 16u &&
                (capture.first % 16u) == 0u &&
                (capture.count % 16u) == 0u;
        }
    }

    state.srv_count = mutation.srv_count;
    for (std::uint32_t i = 0u;
         i < state.srv_count;
         ++i) {
        state.srvs[i].slot =
            mutation.srvs[i].slot;
        context->PSGetShaderResources(
            state.srvs[i].slot,
            1u,
            &state.srvs[i].value);
    }

    state.sampler_count =
        mutation.sampler_count;
    for (std::uint32_t i = 0u;
         i < state.sampler_count;
         ++i) {
        state.samplers[i].slot =
            mutation.samplers[i].slot;
        context->PSGetSamplers(
            state.samplers[i].slot,
            1u,
            &state.samplers[i].value);
    }

    return true;
}

void apply_mutation(
    ID3D11DeviceContext *context,
    const draw_tx_mutation &mutation) noexcept
{
    if (mutation.replace_pixel_shader)
        context->PSSetShader(
            mutation.pixel_shader,
            nullptr,
            0u);

    for (std::uint32_t i = 0u;
         i < mutation.constant_buffer_count;
         ++i) {
        auto *buffer =
            mutation.constant_buffers[i].buffer;
        context->PSSetConstantBuffers(
            mutation.constant_buffers[i].slot,
            1u,
            &buffer);
    }

    for (std::uint32_t i = 0u;
         i < mutation.srv_count;
         ++i) {
        auto *srv =
            mutation.srvs[i].srv;
        context->PSSetShaderResources(
            mutation.srvs[i].slot,
            1u,
            &srv);
    }

    for (std::uint32_t i = 0u;
         i < mutation.sampler_count;
         ++i) {
        auto *sampler =
            mutation.samplers[i].sampler;
        context->PSSetSamplers(
            mutation.samplers[i].slot,
            1u,
            &sampler);
    }
}

bool restore_state(
    ID3D11DeviceContext *context,
    ID3D11DeviceContext1 *context1,
    native_state &state) noexcept
{
    if (context == nullptr) {
        release_native_state(state);
        return false;
    }

    if (state.shader_captured)
        context->PSSetShader(
            state.shader,
            state.classes.data(),
            state.class_count);

    for (std::uint32_t i = 0u;
         i < state.cb_count;
         ++i) {
        const auto &capture =
            state.cbs[i];
        restore_ps_constant_buffer_window(
            context,
            context1,
            capture.slot,
            capture.explicit_window
                ? capture.window
                : capture.base,
            capture.explicit_window,
            capture.first,
            capture.count);
    }

    for (std::uint32_t i = 0u;
         i < state.srv_count;
         ++i) {
        auto *srv =
            state.srvs[i].value;
        context->PSSetShaderResources(
            state.srvs[i].slot,
            1u,
            &srv);
    }

    for (std::uint32_t i = 0u;
         i < state.sampler_count;
         ++i) {
        auto *sampler =
            state.samplers[i].value;
        context->PSSetSamplers(
            state.samplers[i].slot,
            1u,
            &sampler);
    }

    bool verified = true;
    if (telemetry::native_state_verification_enabled()) {
        if (state.shader_captured) {
            ID3D11PixelShader *shader = nullptr;
            std::array<ID3D11ClassInstance *,
                       draw_tx_max_class_instances> classes{};
            UINT class_count =
                static_cast<UINT>(
                    classes.size());
            context->PSGetShader(
                &shader,
                classes.data(),
                &class_count);

            verified =
                shader == state.shader &&
                class_count == state.class_count;

            if (verified) {
                for (UINT i = 0u;
                     i < class_count;
                     ++i) {
                    if (classes[i] !=
                        state.classes[i]) {
                        verified = false;
                        break;
                    }
                }
            }

            if (shader != nullptr)
                shader->Release();
            for (UINT i = 0u;
                 i < class_count &&
                 i < classes.size();
                 ++i)
                if (classes[i] != nullptr)
                    classes[i]->Release();
        }

        for (std::uint32_t i = 0u;
             verified &&
             i < state.cb_count;
             ++i) {
            ID3D11Buffer *buffer = nullptr;
            context->PSGetConstantBuffers(
                state.cbs[i].slot,
                1u,
                &buffer);
            verified =
                buffer ==
                state.cbs[i].base;
            if (buffer != nullptr)
                buffer->Release();

            if (verified &&
                context1 != nullptr &&
                state.cbs[i].explicit_window) {
                ID3D11Buffer *window = nullptr;
                UINT first = 0u;
                UINT count = 0u;
                context1->PSGetConstantBuffers1(
                    state.cbs[i].slot,
                    1u,
                    &window,
                    &first,
                    &count);
                verified =
                    window ==
                        state.cbs[i].window &&
                    first ==
                        state.cbs[i].first &&
                    count ==
                        state.cbs[i].count;
                if (window != nullptr)
                    window->Release();
            }
        }

        for (std::uint32_t i = 0u;
             verified &&
             i < state.srv_count;
             ++i) {
            ID3D11ShaderResourceView *srv = nullptr;
            context->PSGetShaderResources(
                state.srvs[i].slot,
                1u,
                &srv);
            verified =
                srv ==
                    state.srvs[i].value;
            if (srv != nullptr)
                srv->Release();
        }

        for (std::uint32_t i = 0u;
             verified &&
             i < state.sampler_count;
             ++i) {
            ID3D11SamplerState *sampler = nullptr;
            context->PSGetSamplers(
                state.samplers[i].slot,
                1u,
                &sampler);
            verified =
                sampler ==
                    state.samplers[i].value;
            if (sampler != nullptr)
                sampler->Release();
        }
    }

    release_native_state(state);
    return verified;
}

bool write_vtable_slot(
    void **slot,
    void *expected,
    void *replacement) noexcept
{
    if (slot == nullptr ||
        *slot != expected)
        return false;

    DWORD old = 0u;
    if (!VirtualProtect(
            slot,
            sizeof(void *),
            PAGE_READWRITE,
            &old))
        return false;

    InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile *>(slot),
        replacement);

    DWORD ignored = 0u;
    (void)VirtualProtect(
        slot,
        sizeof(void *),
        old,
        &ignored);

    return *slot == replacement;
}

bool restore_vtable_slot(
    void **slot,
    void *replacement,
    void *original) noexcept
{
    if (slot == nullptr)
        return false;

    if (*slot == original)
        return true;

    if (*slot != replacement)
        return false;

    DWORD old = 0u;
    if (!VirtualProtect(
            slot,
            sizeof(void *),
            PAGE_READWRITE,
            &old))
        return false;

    InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile *>(slot),
        original);

    DWORD ignored = 0u;
    (void)VirtualProtect(
        slot,
        sizeof(void *),
        old,
        &ignored);

    return *slot == original;
}

bool pending_matches(
    const pending_draw &pending,
    ID3D11DeviceContext *context,
    pending_kind kind,
    std::uint32_t count,
    std::uint32_t instance_count,
    std::uint32_t first,
    std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept
{
    return
        pending.active &&
        pending.context == context &&
        pending.kind == kind &&
        pending.count == count &&
        pending.instance_count == instance_count &&
        pending.first == first &&
        pending.vertex_offset == vertex_offset &&
        pending.first_instance == first_instance;
}

} // namespace

struct pmetal_native_draw_bridge::impl {
    ID3D11DeviceContext *context = nullptr;
    ID3D11DeviceContext1 *context1 = nullptr;
    void **vtable = nullptr;

    draw_fn original_draw = nullptr;
    draw_indexed_fn original_draw_indexed = nullptr;
    draw_instanced_fn original_draw_instanced = nullptr;
    draw_indexed_instanced_fn
        original_draw_indexed_instanced = nullptr;

    std::atomic<std::uint64_t> armed{0};
    std::atomic<std::uint64_t> draw_applied{0};
    std::atomic<std::uint64_t> draw_indexed_applied{0};
    std::atomic<std::uint64_t> arm_reject{0};
    std::atomic<std::uint64_t> restore_fail{0};
    std::atomic_bool hook_active{false};
    std::atomic_bool quarantined{false};
};

pmetal_native_draw_bridge::~pmetal_native_draw_bridge()
{
    uninstall();
    delete impl_;
    impl_ = nullptr;
}

bool pmetal_native_draw_bridge::install(
    reshade::api::device *device) noexcept
{
    if (device == nullptr)
        return false;

    if (impl_ == nullptr) {
        try {
            impl_ = new impl{};
        } catch (...) {
            return false;
        }
    }

    if (impl_->hook_active.load(
            std::memory_order_acquire))
        return true;

    auto *native_device =
        reinterpret_cast<ID3D11Device *>(
            static_cast<std::uintptr_t>(
                device->get_native()));
    if (native_device == nullptr)
        return false;

    ID3D11DeviceContext *context = nullptr;
    native_device->GetImmediateContext(
        &context);
    if (context == nullptr)
        return false;

    if (context->GetType() !=
        D3D11_DEVICE_CONTEXT_IMMEDIATE) {
        context->Release();
        return false;
    }

    auto **vtable =
        *reinterpret_cast<void ***>(
            context);
    if (vtable == nullptr) {
        context->Release();
        return false;
    }

    auto *original_draw_indexed =
        reinterpret_cast<draw_indexed_fn>(
            vtable[k_vtbl_draw_indexed]);
    auto *original_draw =
        reinterpret_cast<draw_fn>(
            vtable[k_vtbl_draw]);
    auto *original_draw_indexed_instanced =
        reinterpret_cast<draw_indexed_instanced_fn>(
            vtable[
                k_vtbl_draw_indexed_instanced]);
    auto *original_draw_instanced =
        reinterpret_cast<draw_instanced_fn>(
            vtable[k_vtbl_draw_instanced]);

    if (original_draw == nullptr ||
        original_draw_indexed == nullptr ||
        original_draw_instanced == nullptr ||
        original_draw_indexed_instanced == nullptr) {
        context->Release();
        return false;
    }

    ID3D11DeviceContext1 *context1 = nullptr;
    (void)context->QueryInterface(
        __uuidof(ID3D11DeviceContext1),
        reinterpret_cast<void **>(
            &context1));

    pmetal_native_draw_bridge *expected = nullptr;
    if (!g_active.compare_exchange_strong(
            expected,
            this,
            std::memory_order_acq_rel)) {
        if (context1 != nullptr)
            context1->Release();
        context->Release();
        return expected == this;
    }

    impl_->context = context;
    impl_->context1 = context1;
    impl_->vtable = vtable;
    impl_->original_draw = original_draw;
    impl_->original_draw_indexed =
        original_draw_indexed;
    impl_->original_draw_instanced =
        original_draw_instanced;
    impl_->original_draw_indexed_instanced =
        original_draw_indexed_instanced;

    g_original_draw = original_draw;
    g_original_draw_indexed =
        original_draw_indexed;
    g_original_draw_instanced =
        original_draw_instanced;
    g_original_draw_indexed_instanced =
        original_draw_indexed_instanced;

    bool ok =
        write_vtable_slot(
            &vtable[k_vtbl_draw_indexed],
            reinterpret_cast<void *>(
                original_draw_indexed),
            reinterpret_cast<void *>(
                &pmetal_native_draw_bridge::
                    draw_indexed_hook)) &&
        write_vtable_slot(
            &vtable[k_vtbl_draw],
            reinterpret_cast<void *>(
                original_draw),
            reinterpret_cast<void *>(
                &pmetal_native_draw_bridge::
                    draw_hook)) &&
        write_vtable_slot(
            &vtable[
                k_vtbl_draw_indexed_instanced],
            reinterpret_cast<void *>(
                original_draw_indexed_instanced),
            reinterpret_cast<void *>(
                &pmetal_native_draw_bridge::
                    draw_indexed_instanced_hook)) &&
        write_vtable_slot(
            &vtable[k_vtbl_draw_instanced],
            reinterpret_cast<void *>(
                original_draw_instanced),
            reinterpret_cast<void *>(
                &pmetal_native_draw_bridge::
                    draw_instanced_hook));

    if (!ok) {
        (void)restore_vtable_slot(
            &vtable[k_vtbl_draw_indexed],
            reinterpret_cast<void *>(
                &pmetal_native_draw_bridge::
                    draw_indexed_hook),
            reinterpret_cast<void *>(
                original_draw_indexed));
        (void)restore_vtable_slot(
            &vtable[k_vtbl_draw],
            reinterpret_cast<void *>(
                &pmetal_native_draw_bridge::
                    draw_hook),
            reinterpret_cast<void *>(
                original_draw));
        (void)restore_vtable_slot(
            &vtable[
                k_vtbl_draw_indexed_instanced],
            reinterpret_cast<void *>(
                &pmetal_native_draw_bridge::
                    draw_indexed_instanced_hook),
            reinterpret_cast<void *>(
                original_draw_indexed_instanced));
        (void)restore_vtable_slot(
            &vtable[k_vtbl_draw_instanced],
            reinterpret_cast<void *>(
                &pmetal_native_draw_bridge::
                    draw_instanced_hook),
            reinterpret_cast<void *>(
                original_draw_instanced));

        g_active.store(
            nullptr,
            std::memory_order_release);

        if (impl_->context1 != nullptr) {
            impl_->context1->Release();
            impl_->context1 = nullptr;
        }
        impl_->context->Release();
        impl_->context = nullptr;
        impl_->vtable = nullptr;
        return false;
    }

    impl_->hook_active.store(
        true,
        std::memory_order_release);
    return true;
}

void pmetal_native_draw_bridge::uninstall() noexcept
{
    if (impl_ == nullptr)
        return;

    if (!impl_->hook_active.load(
            std::memory_order_acquire)) {
        if (g_pending.owner == this) {
            release_mutation(
                g_pending.mutation);
            g_pending = {};
        }
        return;
    }

    bool restored = true;
    auto **vtable = impl_->vtable;

    restored &=
        restore_vtable_slot(
            &vtable[k_vtbl_draw_indexed],
            reinterpret_cast<void *>(
                &pmetal_native_draw_bridge::
                    draw_indexed_hook),
            reinterpret_cast<void *>(
                impl_->original_draw_indexed));
    restored &=
        restore_vtable_slot(
            &vtable[k_vtbl_draw],
            reinterpret_cast<void *>(
                &pmetal_native_draw_bridge::
                    draw_hook),
            reinterpret_cast<void *>(
                impl_->original_draw));
    restored &=
        restore_vtable_slot(
            &vtable[
                k_vtbl_draw_indexed_instanced],
            reinterpret_cast<void *>(
                &pmetal_native_draw_bridge::
                    draw_indexed_instanced_hook),
            reinterpret_cast<void *>(
                impl_->
                    original_draw_indexed_instanced));
    restored &=
        restore_vtable_slot(
            &vtable[k_vtbl_draw_instanced],
            reinterpret_cast<void *>(
                &pmetal_native_draw_bridge::
                    draw_instanced_hook),
            reinterpret_cast<void *>(
                impl_->original_draw_instanced));

    if (!restored) {
        impl_->restore_fail.fetch_add(
            1u,
            std::memory_order_relaxed);
        impl_->quarantined.store(
            true,
            std::memory_order_release);
        return;
    }

    impl_->hook_active.store(
        false,
        std::memory_order_release);
    g_active.store(
        nullptr,
        std::memory_order_release);

    if (g_pending.owner == this) {
        release_mutation(
            g_pending.mutation);
        g_pending = {};
    }

    if (impl_->context1 != nullptr) {
        impl_->context1->Release();
        impl_->context1 = nullptr;
    }
    if (impl_->context != nullptr) {
        impl_->context->Release();
        impl_->context = nullptr;
    }
    impl_->vtable = nullptr;

    g_original_draw = nullptr;
    g_original_draw_indexed = nullptr;
    g_original_draw_instanced = nullptr;
    g_original_draw_indexed_instanced = nullptr;
}

bool pmetal_native_draw_bridge::arm_draw(
    reshade::api::command_list *cmd_list,
    const draw_tx_mutation &mutation,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance) noexcept
{
    if (impl_ == nullptr ||
        !impl_->hook_active.load(
            std::memory_order_acquire) ||
        impl_->quarantined.load(
            std::memory_order_acquire) ||
        cmd_list == nullptr ||
        (mutation.owners &
         core::operator_bit(
             core::operator_id::env_spec)) == 0u) {
        if (impl_ != nullptr)
            impl_->arm_reject.fetch_add(
                1u,
                std::memory_order_relaxed);
        return false;
    }

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            static_cast<std::uintptr_t>(
                cmd_list->get_native()));
    if (context == nullptr ||
        context != impl_->context ||
        context->GetType() !=
            D3D11_DEVICE_CONTEXT_IMMEDIATE) {
        impl_->arm_reject.fetch_add(
            1u,
            std::memory_order_relaxed);
        return false;
    }

    if (g_pending.active) {
        // Another addon may cancel a ReShade draw after this addon armed the
        // original-context bridge. In that case no native Draw follows and
        // the pending mutation is harmless but stale. Drop it before arming
        // the next exact draw instead of poisoning the bridge globally.
        release_mutation(
            g_pending.mutation);
        g_pending = {};
        impl_->arm_reject.fetch_add(
            1u,
            std::memory_order_relaxed);
    }

    g_pending.owner = this;
    g_pending.context = context;
    g_pending.mutation = mutation;
    retain_mutation(
        g_pending.mutation);
    g_pending.kind =
        pending_kind::draw;
    g_pending.count =
        vertex_count;
    g_pending.instance_count =
        instance_count;
    g_pending.first =
        first_vertex;
    g_pending.first_instance =
        first_instance;
    g_pending.active = true;

    impl_->armed.fetch_add(
        1u,
        std::memory_order_relaxed);
    return true;
}

bool pmetal_native_draw_bridge::arm_draw_indexed(
    reshade::api::command_list *cmd_list,
    const draw_tx_mutation &mutation,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept
{
    if (impl_ == nullptr ||
        !impl_->hook_active.load(
            std::memory_order_acquire) ||
        impl_->quarantined.load(
            std::memory_order_acquire) ||
        cmd_list == nullptr ||
        (mutation.owners &
         core::operator_bit(
             core::operator_id::env_spec)) == 0u) {
        if (impl_ != nullptr)
            impl_->arm_reject.fetch_add(
                1u,
                std::memory_order_relaxed);
        return false;
    }

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            static_cast<std::uintptr_t>(
                cmd_list->get_native()));
    if (context == nullptr ||
        context != impl_->context ||
        context->GetType() !=
            D3D11_DEVICE_CONTEXT_IMMEDIATE) {
        impl_->arm_reject.fetch_add(
            1u,
            std::memory_order_relaxed);
        return false;
    }

    if (g_pending.active) {
        // Another addon may cancel a ReShade draw after this addon armed the
        // original-context bridge. In that case no native Draw follows and
        // the pending mutation is harmless but stale. Drop it before arming
        // the next exact draw instead of poisoning the bridge globally.
        release_mutation(
            g_pending.mutation);
        g_pending = {};
        impl_->arm_reject.fetch_add(
            1u,
            std::memory_order_relaxed);
    }

    g_pending.owner = this;
    g_pending.context = context;
    g_pending.mutation = mutation;
    retain_mutation(
        g_pending.mutation);
    g_pending.kind =
        pending_kind::draw_indexed;
    g_pending.count =
        index_count;
    g_pending.instance_count =
        instance_count;
    g_pending.first =
        first_index;
    g_pending.vertex_offset =
        vertex_offset;
    g_pending.first_instance =
        first_instance;
    g_pending.active = true;

    impl_->armed.fetch_add(
        1u,
        std::memory_order_relaxed);
    return true;
}

void STDMETHODCALLTYPE
pmetal_native_draw_bridge::draw_hook(
    ID3D11DeviceContext *context,
    UINT vertex_count,
    UINT start_vertex) noexcept
{
    auto *bridge =
        g_active.load(
            std::memory_order_acquire);
    auto original =
        g_original_draw;

    if (bridge == nullptr ||
        bridge->impl_ == nullptr ||
        original == nullptr ||
        !pending_matches(
            g_pending,
            context,
            pending_kind::draw,
            vertex_count,
            1u,
            start_vertex,
            0,
            0u)) {
        if (original != nullptr)
            original(
                context,
                vertex_count,
                start_vertex);
        return;
    }

    pending_draw pending = g_pending;
    g_pending = {};

    native_state state{};
    const bool captured =
        capture_state(
            context,
            bridge->impl_->context1,
            pending.mutation,
            state);

    if (!captured) {
        bridge->impl_->arm_reject.fetch_add(
            1u,
            std::memory_order_relaxed);
        original(
            context,
            vertex_count,
            start_vertex);
        release_mutation(
            pending.mutation);
        return;
    }

    apply_mutation(
        context,
        pending.mutation);
    original(
        context,
        vertex_count,
        start_vertex);

    if (!restore_state(
            context,
            bridge->impl_->context1,
            state)) {
        bridge->impl_->restore_fail.fetch_add(
            1u,
            std::memory_order_relaxed);
        bridge->impl_->quarantined.store(
            true,
            std::memory_order_release);
    } else {
        bridge->impl_->draw_applied.fetch_add(
            1u,
            std::memory_order_relaxed);
    }

    release_mutation(
        pending.mutation);
}

void STDMETHODCALLTYPE
pmetal_native_draw_bridge::draw_indexed_hook(
    ID3D11DeviceContext *context,
    UINT index_count,
    UINT start_index,
    INT base_vertex) noexcept
{
    auto *bridge =
        g_active.load(
            std::memory_order_acquire);
    auto original =
        g_original_draw_indexed;

    if (bridge == nullptr ||
        bridge->impl_ == nullptr ||
        original == nullptr ||
        !pending_matches(
            g_pending,
            context,
            pending_kind::draw_indexed,
            index_count,
            1u,
            start_index,
            base_vertex,
            0u)) {
        if (original != nullptr)
            original(
                context,
                index_count,
                start_index,
                base_vertex);
        return;
    }

    pending_draw pending = g_pending;
    g_pending = {};

    native_state state{};
    const bool captured =
        capture_state(
            context,
            bridge->impl_->context1,
            pending.mutation,
            state);

    if (!captured) {
        bridge->impl_->arm_reject.fetch_add(
            1u,
            std::memory_order_relaxed);
        original(
            context,
            index_count,
            start_index,
            base_vertex);
        release_mutation(
            pending.mutation);
        return;
    }

    apply_mutation(
        context,
        pending.mutation);
    original(
        context,
        index_count,
        start_index,
        base_vertex);

    if (!restore_state(
            context,
            bridge->impl_->context1,
            state)) {
        bridge->impl_->restore_fail.fetch_add(
            1u,
            std::memory_order_relaxed);
        bridge->impl_->quarantined.store(
            true,
            std::memory_order_release);
    } else {
        bridge->impl_->
            draw_indexed_applied.fetch_add(
                1u,
                std::memory_order_relaxed);
    }

    release_mutation(
        pending.mutation);
}

void STDMETHODCALLTYPE
pmetal_native_draw_bridge::draw_instanced_hook(
    ID3D11DeviceContext *context,
    UINT vertex_count_per_instance,
    UINT instance_count,
    UINT start_vertex,
    UINT start_instance) noexcept
{
    auto *bridge =
        g_active.load(
            std::memory_order_acquire);
    auto original =
        g_original_draw_instanced;

    if (bridge == nullptr ||
        bridge->impl_ == nullptr ||
        original == nullptr ||
        !pending_matches(
            g_pending,
            context,
            pending_kind::draw,
            vertex_count_per_instance,
            instance_count,
            start_vertex,
            0,
            start_instance)) {
        if (original != nullptr)
            original(
                context,
                vertex_count_per_instance,
                instance_count,
                start_vertex,
                start_instance);
        return;
    }

    pending_draw pending = g_pending;
    g_pending = {};

    native_state state{};
    const bool captured =
        capture_state(
            context,
            bridge->impl_->context1,
            pending.mutation,
            state);

    if (!captured) {
        bridge->impl_->arm_reject.fetch_add(
            1u,
            std::memory_order_relaxed);
        original(
            context,
            vertex_count_per_instance,
            instance_count,
            start_vertex,
            start_instance);
        release_mutation(
            pending.mutation);
        return;
    }

    apply_mutation(
        context,
        pending.mutation);
    original(
        context,
        vertex_count_per_instance,
        instance_count,
        start_vertex,
        start_instance);

    if (!restore_state(
            context,
            bridge->impl_->context1,
            state)) {
        bridge->impl_->restore_fail.fetch_add(
            1u,
            std::memory_order_relaxed);
        bridge->impl_->quarantined.store(
            true,
            std::memory_order_release);
    } else {
        bridge->impl_->draw_applied.fetch_add(
            1u,
            std::memory_order_relaxed);
    }

    release_mutation(
        pending.mutation);
}

void STDMETHODCALLTYPE
pmetal_native_draw_bridge::draw_indexed_instanced_hook(
    ID3D11DeviceContext *context,
    UINT index_count_per_instance,
    UINT instance_count,
    UINT start_index,
    INT base_vertex,
    UINT start_instance) noexcept
{
    auto *bridge =
        g_active.load(
            std::memory_order_acquire);
    auto original =
        g_original_draw_indexed_instanced;

    if (bridge == nullptr ||
        bridge->impl_ == nullptr ||
        original == nullptr ||
        !pending_matches(
            g_pending,
            context,
            pending_kind::draw_indexed,
            index_count_per_instance,
            instance_count,
            start_index,
            base_vertex,
            start_instance)) {
        if (original != nullptr)
            original(
                context,
                index_count_per_instance,
                instance_count,
                start_index,
                base_vertex,
                start_instance);
        return;
    }

    pending_draw pending = g_pending;
    g_pending = {};

    native_state state{};
    const bool captured =
        capture_state(
            context,
            bridge->impl_->context1,
            pending.mutation,
            state);

    if (!captured) {
        bridge->impl_->arm_reject.fetch_add(
            1u,
            std::memory_order_relaxed);
        original(
            context,
            index_count_per_instance,
            instance_count,
            start_index,
            base_vertex,
            start_instance);
        release_mutation(
            pending.mutation);
        return;
    }

    apply_mutation(
        context,
        pending.mutation);
    original(
        context,
        index_count_per_instance,
        instance_count,
        start_index,
        base_vertex,
        start_instance);

    if (!restore_state(
            context,
            bridge->impl_->context1,
            state)) {
        bridge->impl_->restore_fail.fetch_add(
            1u,
            std::memory_order_relaxed);
        bridge->impl_->quarantined.store(
            true,
            std::memory_order_release);
    } else {
        bridge->impl_->
            draw_indexed_applied.fetch_add(
                1u,
                std::memory_order_relaxed);
    }

    release_mutation(
        pending.mutation);
}

pmetal_native_draw_telemetry
pmetal_native_draw_bridge::telemetry() const noexcept
{
    if (impl_ == nullptr)
        return {};

    return {
        impl_->armed.load(
            std::memory_order_relaxed),
        impl_->draw_applied.load(
            std::memory_order_relaxed),
        impl_->draw_indexed_applied.load(
            std::memory_order_relaxed),
        impl_->arm_reject.load(
            std::memory_order_relaxed),
        impl_->restore_fail.load(
            std::memory_order_relaxed),
        impl_->hook_active.load(
            std::memory_order_relaxed),
        impl_->quarantined.load(
            std::memory_order_relaxed)
    };
}

void pmetal_native_draw_bridge::reset_telemetry() noexcept
{
    if (impl_ == nullptr)
        return;

    impl_->armed.store(
        0u,
        std::memory_order_relaxed);
    impl_->draw_applied.store(
        0u,
        std::memory_order_relaxed);
    impl_->draw_indexed_applied.store(
        0u,
        std::memory_order_relaxed);
    impl_->arm_reject.store(
        0u,
        std::memory_order_relaxed);
    impl_->restore_fail.store(
        0u,
        std::memory_order_relaxed);
}

} // namespace dsrrl::runtime
