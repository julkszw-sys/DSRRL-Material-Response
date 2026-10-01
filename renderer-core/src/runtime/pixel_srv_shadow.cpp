#include "dsrrl/runtime/pixel_srv_shadow.hpp"

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
#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime {
namespace {

struct shadow_record {
    void *command_list_key = nullptr;
    std::array<ID3D11ShaderResourceView *,k_pixel_srv_shadow_slots> srvs{};
    std::array<ID3D11SamplerState *,k_pixel_sampler_shadow_slots> samplers{};
    std::array<pixel_cb_shadow_binding,k_pixel_cb_shadow_slots> cbs{};
    std::uint32_t srv_valid_mask = 0u;
    std::uint32_t sampler_valid_mask = 0u;
    std::uint32_t cb_valid_mask = 0u;
    std::uint64_t serial = 0u;
};

constexpr std::size_t k_shadow_records = 8u;
thread_local std::array<shadow_record,k_shadow_records> g_records{};
thread_local std::uint64_t g_serial = 0u;

shadow_record &record_for(void *key) noexcept
{
    const auto raw =
        reinterpret_cast<std::uintptr_t>(key);
    const auto index =
        static_cast<std::size_t>(
            ((raw >> 4u) ^ (raw >> 13u)) &
            (k_shadow_records - 1u));

    auto &record = g_records[index];
    if (record.command_list_key != key) {
        record = {};
        record.command_list_key = key;
    }
    return record;
}

} // namespace

void pixel_srv_shadow_reset() noexcept
{
    g_records = {};
    g_serial = 0u;
}

void pixel_srv_shadow_on_push_descriptors(
    reshade::api::command_list *cmd_list,
    reshade::api::shader_stage stages,
    reshade::api::pipeline_layout,
    std::uint32_t,
    const reshade::api::descriptor_table_update &update) noexcept
{
    if (cmd_list == nullptr ||
        (stages & reshade::api::shader_stage::pixel) !=
            reshade::api::shader_stage::pixel ||
        update.descriptors == nullptr ||
        update.count == 0u)
        return;

    const bool srv_update =
        update.type ==
            reshade::api::descriptor_type::shader_resource_view;
    const bool sampler_update =
        update.type ==
            reshade::api::descriptor_type::sampler;
    const bool cb_update =
        update.type ==
            reshade::api::descriptor_type::constant_buffer;
    if (!srv_update && !sampler_update &&
        !cb_update)
        return;

    void *const key =
        reinterpret_cast<void *>(
            static_cast<std::uintptr_t>(
                cmd_list->get_native()));
    if (key == nullptr)
        return;

    auto &record =
        record_for(key);

    if (srv_update) {
        if (update.binding >=
            k_pixel_srv_shadow_slots)
            return;

        const auto *views =
            static_cast<
                const reshade::api::resource_view *>(
                    update.descriptors);
        const auto count =
            std::min<std::uint32_t>(
                update.count,
                k_pixel_srv_shadow_slots -
                    update.binding);

        for (std::uint32_t i = 0u;
             i < count;
             ++i) {
            const auto slot =
                update.binding + i;
            record.srvs[slot] =
                reinterpret_cast<
                    ID3D11ShaderResourceView *>(
                        static_cast<std::uintptr_t>(
                            views[i].handle));
            record.srv_valid_mask |=
                std::uint32_t{1u} << slot;
        }
    } else if (sampler_update) {
        if (update.binding >=
            k_pixel_sampler_shadow_slots)
            return;

        const auto *samplers =
            static_cast<
                const reshade::api::sampler *>(
                    update.descriptors);
        const auto count =
            std::min<std::uint32_t>(
                update.count,
                k_pixel_sampler_shadow_slots -
                    update.binding);

        for (std::uint32_t i = 0u;
             i < count;
             ++i) {
            const auto slot =
                update.binding + i;
            record.samplers[slot] =
                reinterpret_cast<
                    ID3D11SamplerState *>(
                        static_cast<std::uintptr_t>(
                            samplers[i].handle));
            record.sampler_valid_mask |=
                std::uint32_t{1u} << slot;
        }
    } else {
        if (update.binding >=
            k_pixel_cb_shadow_slots)
            return;

        const auto *ranges =
            static_cast<
                const reshade::api::buffer_range *>(
                    update.descriptors);
        const auto count =
            std::min<std::uint32_t>(
                update.count,
                k_pixel_cb_shadow_slots -
                    update.binding);

        for (std::uint32_t i = 0u;
             i < count;
             ++i) {
            const auto slot =
                update.binding + i;
            record.cbs[slot] = {
                reinterpret_cast<ID3D11Buffer *>(
                    static_cast<std::uintptr_t>(
                        ranges[i].buffer.handle)),
                ranges[i].offset,
                ranges[i].size
            };
            record.cb_valid_mask |=
                std::uint32_t{1u} << slot;
        }
    }

    record.serial = ++g_serial;
}

bool pixel_srv_shadow_snapshot(
    reshade::api::command_list *cmd_list,
    std::uint32_t first,
    std::uint32_t count,
    ID3D11ShaderResourceView **out) noexcept
{
    if (cmd_list == nullptr ||
        out == nullptr ||
        count == 0u ||
        first >= k_pixel_srv_shadow_slots ||
        first + count > k_pixel_srv_shadow_slots)
        return false;

    void *const key =
        reinterpret_cast<void *>(
            static_cast<std::uintptr_t>(
                cmd_list->get_native()));
    if (key == nullptr)
        return false;

    const auto raw =
        reinterpret_cast<std::uintptr_t>(key);
    const auto index =
        static_cast<std::size_t>(
            ((raw >> 4u) ^ (raw >> 13u)) &
            (k_shadow_records - 1u));
    const auto &record =
        g_records[index];

    if (record.command_list_key != key ||
        record.serial == 0u)
        return false;

    const auto requested_mask =
        ((std::uint32_t{1u} << count) - 1u) <<
        first;
    if ((record.srv_valid_mask &
         requested_mask) !=
        requested_mask)
        return false;

    for (std::uint32_t i = 0u; i < count; ++i)
        out[i] = record.srvs[first + i];

    return true;
}

bool pixel_sampler_shadow_snapshot(
    reshade::api::command_list *cmd_list,
    std::uint32_t first,
    std::uint32_t count,
    ID3D11SamplerState **out) noexcept
{
    if (cmd_list == nullptr ||
        out == nullptr ||
        count == 0u ||
        first >= k_pixel_sampler_shadow_slots ||
        first + count > k_pixel_sampler_shadow_slots)
        return false;

    void *const key =
        reinterpret_cast<void *>(
            static_cast<std::uintptr_t>(
                cmd_list->get_native()));
    if (key == nullptr)
        return false;

    const auto raw =
        reinterpret_cast<std::uintptr_t>(key);
    const auto index =
        static_cast<std::size_t>(
            ((raw >> 4u) ^ (raw >> 13u)) &
            (k_shadow_records - 1u));
    const auto &record =
        g_records[index];

    if (record.command_list_key != key ||
        record.serial == 0u)
        return false;

    const auto requested_mask =
        ((std::uint32_t{1u} << count) - 1u) <<
        first;
    if ((record.sampler_valid_mask &
         requested_mask) !=
        requested_mask)
        return false;

    for (std::uint32_t i = 0u;
         i < count;
         ++i)
        out[i] =
            record.samplers[first + i];

    return true;
}

bool pixel_cb_shadow_snapshot(
    reshade::api::command_list *cmd_list,
    std::uint32_t first,
    std::uint32_t count,
    pixel_cb_shadow_binding *out) noexcept
{
    if (cmd_list == nullptr ||
        out == nullptr ||
        count == 0u ||
        first >= k_pixel_cb_shadow_slots ||
        first + count > k_pixel_cb_shadow_slots)
        return false;

    void *const key =
        reinterpret_cast<void *>(
            static_cast<std::uintptr_t>(
                cmd_list->get_native()));
    if (key == nullptr)
        return false;

    const auto raw =
        reinterpret_cast<std::uintptr_t>(key);
    const auto index =
        static_cast<std::size_t>(
            ((raw >> 4u) ^ (raw >> 13u)) &
            (k_shadow_records - 1u));
    const auto &record =
        g_records[index];

    if (record.command_list_key != key ||
        record.serial == 0u)
        return false;

    const auto requested_mask =
        ((std::uint32_t{1u} << count) - 1u) <<
        first;
    if ((record.cb_valid_mask &
         requested_mask) !=
        requested_mask)
        return false;

    for (std::uint32_t i = 0u;
         i < count;
         ++i)
        out[i] =
            record.cbs[first + i];

    return true;
}

} // namespace dsrrl::runtime
