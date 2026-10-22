#pragma once

#include "dsrrl/core/types.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/runtime/draw_state_transaction.hpp"
#include "dsrrl/runtime/island_draw_adapter.hpp"

#include <reshade.hpp>

#if RESHADE_API_VERSION != 20
#error DSRRL Material Response draw transaction requires ReShade Add-on API 20
#endif

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11PixelShader;

namespace dsrrl::runtime {

enum class material_response_replacement_family : std::uint8_t {
    stable = 0,
    stable_upper_lower,
    hemenvlerp,
    hemenvlerp_upper_lower
};

struct prepared_material_response_draw {
    island_draw_adapter_request request{};
    ID3D11PixelShader *shader = nullptr;
    ID3D11Buffer *b12 = nullptr;
    std::uint32_t receiver_id = 0u;
    core::operator_mask replacement_composed_owners = 0u;
    material_response_replacement_family family =
        material_response_replacement_family::stable;
    bool ready = false;
};

struct material_response_draw_telemetry {
    std::uint64_t replacement_register_ok = 0;
    std::uint64_t replacement_register_fail = 0;
    std::uint64_t combined_ul_register_ok = 0;
    std::uint64_t combined_ul_register_fail = 0;
    std::uint64_t combined_ul_prepare = 0;
    std::uint64_t combined_ul_miss = 0;
    std::uint64_t b12_create = 0;
    std::uint64_t b12_hit = 0;
    std::uint64_t b12_bind_fail = 0;
    std::uint64_t eligible_draws = 0;
    std::uint64_t replacement_miss = 0;
    std::uint64_t replay_ok = 0;
    std::uint64_t replay_restore_fail = 0;
    bool quarantined = false;
};

class material_response_draw_runtime {
public:
    explicit material_response_draw_runtime(
        draw_state_transaction_runtime &transactions) noexcept;
    ~material_response_draw_runtime();

    material_response_draw_runtime(
        const material_response_draw_runtime &) = delete;
    material_response_draw_runtime &operator=(
        const material_response_draw_runtime &) = delete;

    void on_init_device(reshade::api::device *device) noexcept;
    void on_destroy_device(reshade::api::device *device) noexcept;

    bool register_receiver_replacement(
        std::uint32_t receiver_id,
        const void *dxbc,
        std::size_t dxbc_size,
        core::operator_mask composed_owners) noexcept;

    bool has_receiver_replacement(
        std::uint32_t receiver_id) const noexcept;

    bool register_lerp_receiver_replacement(
        std::uint32_t receiver_id,
        const void *dxbc,
        std::size_t dxbc_size,
        core::operator_mask composed_owners) noexcept;

    bool has_lerp_receiver_replacement(
        std::uint32_t receiver_id) const noexcept;

    bool register_lerp_receiver_upper_lower_replacement(
        std::uint32_t receiver_id,
        const void *dxbc,
        std::size_t dxbc_size,
        core::operator_mask composed_owners) noexcept;

    bool has_lerp_receiver_upper_lower_replacement(
        std::uint32_t receiver_id) const noexcept;

    bool register_receiver_upper_lower_replacement(
        std::uint32_t receiver_id,
        const void *dxbc,
        std::size_t dxbc_size,
        core::operator_mask composed_owners) noexcept;

    bool has_receiver_upper_lower_replacement(
        std::uint32_t receiver_id) const noexcept;

    // Dedicated exact-Subsurface paired SpecRGB consumer. It is derived from
    // the stable MR target and therefore preserves stock DSR Upper/Lower.
    // This bank is intentionally unreachable from generic MR draw preparation.
    bool register_subsurface_spec_replacement(
        std::uint32_t receiver_id,
        const void *dxbc,
        std::size_t dxbc_size,
        core::operator_mask composed_owners) noexcept;

    // Exact equipment-only legacy spec/material consumer. These banks are
    // never selected by generic MR preparation. Draw-local promotion requires
    // exact owner+slot+SpecRGB companion proof from material_resource_runtime.
    bool register_equipment_spec_replacement(
        std::uint32_t receiver_id,
        const void *dxbc,
        std::size_t dxbc_size,
        core::operator_mask composed_owners) noexcept;

    bool register_equipment_lerp_spec_replacement(
        std::uint32_t receiver_id,
        const void *dxbc,
        std::size_t dxbc_size,
        core::operator_mask composed_owners) noexcept;

    bool prepare_draw_request(
        const operators::material_response::decision &decision,
        prepared_material_response_draw &prepared) noexcept;

    bool prepare_draw_request_with_upper_lower(
        const operators::material_response::decision &decision,
        ID3D11Buffer *b13,
        prepared_material_response_draw &prepared) noexcept;

    bool prepare_lerp_draw_request(
        const operators::material_response::decision &decision,
        prepared_material_response_draw &prepared) noexcept;

    bool prepare_lerp_draw_request_with_upper_lower(
        const operators::material_response::decision &decision,
        ID3D11Buffer *b13,
        prepared_material_response_draw &prepared) noexcept;

    // Carrier preparation only. Authorization must already have been
    // established by an operator-specific exact route (for example the
    // certified DSBT->DSB Subsurface route). This function never infers
    // ownership from the target PTDE donor.
    bool prepare_prevalidated_route_request(
        std::uint32_t receiver_id,
        std::uint32_t route_index,
        prepared_material_response_draw &prepared) noexcept;

    // Same exact prevalidated route, but require the already-certified
    // MR+UpperLower combined receiver replacement and bind the fresh b13
    // LightBank carrier in the same draw transaction.
    bool prepare_prevalidated_route_request_with_upper_lower(
        std::uint32_t receiver_id,
        std::uint32_t route_index,
        ID3D11Buffer *b13,
        prepared_material_response_draw &prepared) noexcept;

    // Only the exact DSBT->DSB Subsurface route may call this. Generic MR has
    // no paired-SpecRGB promotion entry point after the reset.
    bool promote_prevalidated_subsurface_to_spec_rgb(
        prepared_material_response_draw &prepared) noexcept;

    bool promote_prevalidated_equipment_to_spec_rgb(
        prepared_material_response_draw &prepared) noexcept;

    // Carrier-only seam for an independently authorized operator that shares
    // the exact Material Response b12 ABI (e.g. fixed direct PTDE PointLight).
    // This performs no receiver/material authorization and no shader bind:
    // caller must already hold an exact draw decision and compose b12 into its
    // own atomic draw transaction. Returned buffer is AddRef-owned by caller.
    bool prepare_b12_carrier(
        const operators::material_response::decision &decision,
        ID3D11Buffer *&b12) noexcept;

    void release_prepared_draw(
        prepared_material_response_draw &prepared) noexcept;

    void account_dispatch_result(
        draw_tx_result result) noexcept;

    bool replay_draw(
        reshade::api::command_list *cmd_list,
        const operators::material_response::decision &decision,
        std::uint32_t vertex_count,
        std::uint32_t instance_count,
        std::uint32_t first_vertex,
        std::uint32_t first_instance) noexcept;

    bool replay_draw_indexed(
        reshade::api::command_list *cmd_list,
        const operators::material_response::decision &decision,
        std::uint32_t index_count,
        std::uint32_t instance_count,
        std::uint32_t first_index,
        std::int32_t vertex_offset,
        std::uint32_t first_instance) noexcept;

    material_response_draw_telemetry telemetry() const noexcept;
    void reset() noexcept;

private:
    struct replacement_record {
        ID3D11PixelShader *shader = nullptr;
        core::operator_mask composed_owners = 0;
        core::sha256_digest payload_sha256{};
        std::size_t payload_size = 0u;
    };

    enum class replacement_bank : std::uint8_t {
        stable = 0,
        lerp,
        lerp_upper_lower,
        upper_lower,
        subsurface_spec,
        equipment_spec,
        equipment_lerp_spec
    };

    bool acquire_replacement(
        replacement_bank bank,
        std::uint32_t receiver_id,
        replacement_record &replacement) const noexcept;

    bool register_replacement_record(
        std::unordered_map<std::uint32_t, replacement_record> &bank,
        std::uint32_t receiver_id,
        const void *dxbc,
        std::size_t dxbc_size,
        core::operator_mask composed_owners,
        std::atomic<std::uint64_t> &ok_counter,
        std::atomic<std::uint64_t> &fail_counter) noexcept;

    ID3D11Buffer *realize_b12(
        const operators::material_response::decision &decision) noexcept;

    void release_resources() noexcept;

    draw_state_transaction_runtime &transactions_;
    mutable std::mutex mutex_;
    ID3D11Device *device_ = nullptr;
    std::unordered_map<std::uint32_t, replacement_record> replacements_;
    std::unordered_map<std::uint32_t, replacement_record>
        lerp_replacements_;
    std::unordered_map<std::uint32_t, replacement_record>
        lerp_upper_lower_replacements_;
    std::unordered_map<std::uint32_t, replacement_record>
        upper_lower_replacements_;
    std::unordered_map<std::uint32_t, replacement_record>
        subsurface_spec_replacements_;
    std::unordered_map<std::uint32_t, replacement_record>
        equipment_spec_replacements_;
    std::unordered_map<std::uint32_t, replacement_record>
        equipment_lerp_spec_replacements_;
    std::unordered_map<std::uint32_t, ID3D11Buffer *> b12_by_route_;
    std::atomic<std::uint64_t> resource_epoch_{1u};

    std::atomic<std::uint64_t> replacement_register_ok_{0};
    std::atomic<std::uint64_t> replacement_register_fail_{0};
    std::atomic<std::uint64_t> combined_ul_register_ok_{0};
    std::atomic<std::uint64_t> combined_ul_register_fail_{0};
    std::atomic<std::uint64_t> combined_ul_prepare_{0};
    std::atomic<std::uint64_t> combined_ul_miss_{0};
    std::atomic<std::uint64_t> b12_create_{0};
    std::atomic<std::uint64_t> b12_hit_{0};
    std::atomic<std::uint64_t> b12_bind_fail_{0};
    std::atomic<std::uint64_t> eligible_draws_{0};
    std::atomic<std::uint64_t> replacement_miss_{0};
    std::atomic<std::uint64_t> replay_ok_{0};
    std::atomic<std::uint64_t> replay_restore_fail_{0};
    std::atomic_bool local_quarantine_{false};
};

} // namespace dsrrl::runtime
