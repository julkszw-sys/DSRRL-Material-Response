#pragma once

#include "carrier_abi.hpp"
#include "draw_transaction.hpp"
#include "feature_registry.hpp"
#include "hook_registry.hpp"
#include "island_policy.hpp"
#include "receiver_registry.hpp"
#include "shader_registry.hpp"
#include "snapshot_bus.hpp"

#include <cstdint>
#include <optional>

namespace dsrrl::core {

struct island_request {
    operator_id op = operator_id::material_response;
    std::uint32_t carrier_write_mask = 0;
    bool shader_replacement = false;
    bool resource_replacement = false;
};

struct draw_context {
    std::uint64_t command = 0;
    std::uint64_t draw_serial = 0;
    context_kind context = context_kind::unknown;
    std::uint64_t shader_fast_hash = 0;
    sha256_digest shader_sha256{};
    activation_context activation{};
};

class renderer_core {
public:
    renderer_core() = default;

    feature_registry &features() noexcept { return features_; }
    hook_registry &hooks() noexcept { return hooks_; }
    receiver_registry &receivers() noexcept { return receivers_; }
    snapshot_bus &snapshots() noexcept { return snapshots_; }
    shader_registry &shaders() noexcept { return shaders_; }
    draw_transaction_manager &transactions() noexcept { return transactions_; }

    const feature_registry &features() const noexcept { return features_; }
    const hook_registry &hooks() const noexcept { return hooks_; }
    const receiver_registry &receivers() const noexcept { return receivers_; }
    const snapshot_bus &snapshots() const noexcept { return snapshots_; }
    const shader_registry &shaders() const noexcept { return shaders_; }
    const draw_transaction_manager &transactions() const noexcept { return transactions_; }

    render_patch_plan build_plan(
        const draw_context &context,
        const island_request *requests,
        std::uint32_t request_count) const;

    bool phase0_pass_through() const noexcept;

private:
    feature_registry features_;
    hook_registry hooks_;
    receiver_registry receivers_;
    snapshot_bus snapshots_;
    shader_registry shaders_;
    draw_transaction_manager transactions_;
};

} // namespace dsrrl::core
