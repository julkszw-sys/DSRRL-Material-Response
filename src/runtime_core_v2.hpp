#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace dsrrl::runtime_v2 {

enum class operator_kind : std::uint8_t {
    specrgb = 0,
    diffuse,
    normal,
    envspec,
    subsurf,
    count
};

constexpr std::size_t operator_count = static_cast<std::size_t>(operator_kind::count);

enum route_evidence : std::uint32_t {
    evidence_none       = 0,
    evidence_shader     = 1u << 0,
    evidence_receiver   = 1u << 1,
    evidence_material   = 1u << 2,
    evidence_resource   = 1u << 3,
    evidence_logical_id = 1u << 4,
    evidence_format     = 1u << 5,
    evidence_state      = 1u << 6,
};

struct route_contract {
    std::uint32_t required = evidence_none;
};

struct route_observation {
    std::uint32_t verified = evidence_none;
};

struct route_decision {
    bool allowed = false;
    std::uint32_t missing = evidence_none;
};

struct operator_counters {
    std::uint64_t receiver_matches = 0;
    std::uint64_t resource_matches = 0;
    std::uint64_t activations = 0;
    std::uint64_t restores = 0;
    std::uint64_t fail_open = 0;
    std::uint64_t rejected = 0;
    std::uint64_t stale_view = 0;
    std::uint64_t unrestored = 0;
    std::uint64_t restore_faults = 0;
};

struct resource_identity {
    std::uint64_t handle = 0;
    std::uint32_t generation = 0;
    std::uint64_t desc_hash = 0;
    std::uint64_t logical_hash = 0;
};

struct pipeline_identity {
    std::uint64_t handle = 0;
    std::uint32_t generation = 0;
    std::uint64_t pixel_shader_hash = 0;
    std::uint32_t receiver_id = 0;
    std::uint64_t consumer_family_hash = 0;
    bool confirmed = false;
};

struct command_snapshot {
    std::uint64_t command = 0;
    std::uint64_t bound_pipeline = 0;
    std::uint32_t bound_pipeline_generation = 0;
    std::uint64_t draw_serial = 0;
    bool transaction_active = false;
    operator_kind active_operator = operator_kind::specrgb;
};

struct runtime_snapshot {
    std::array<operator_counters, operator_count> operators{};
    std::uint64_t live_resources = 0;
    std::uint64_t live_views = 0;
    std::uint64_t live_pipelines = 0;
    std::uint64_t live_commands = 0;
};

class tracker {
public:
    tracker() = default;

    route_decision evaluate_route(route_contract contract, route_observation observation) const noexcept;

    std::uint32_t init_resource(std::uint64_t handle, std::uint64_t desc_hash, std::uint64_t logical_hash = 0);
    void destroy_resource(std::uint64_t handle);
    bool init_view(std::uint64_t view, std::uint64_t resource);
    void destroy_view(std::uint64_t view);
    std::optional<resource_identity> resolve_view(std::uint64_t view, operator_kind op);

    std::uint32_t init_pipeline(std::uint64_t handle, std::uint64_t pixel_shader_hash,
                                std::uint32_t receiver_id, std::uint64_t consumer_family_hash,
                                bool confirmed);
    void destroy_pipeline(std::uint64_t handle);
    std::optional<pipeline_identity> resolve_pipeline(std::uint64_t handle) const;

    void init_command(std::uint64_t command);
    void destroy_command(std::uint64_t command);
    bool bind_pipeline(std::uint64_t command, std::uint64_t pipeline);
    std::uint64_t begin_draw(std::uint64_t command);
    std::optional<command_snapshot> command_state(std::uint64_t command) const;
    std::optional<pipeline_identity> resolve_bound_pipeline(std::uint64_t command) const;

    void note_receiver_match(operator_kind op);
    void note_resource_match(operator_kind op);

    bool begin_transaction(std::uint64_t command, operator_kind op,
                           route_contract contract, route_observation observation);
    bool restore_transaction(std::uint64_t command, operator_kind op);
    void note_fail_open(operator_kind op);

    runtime_snapshot seal_frame();
    runtime_snapshot snapshot() const;
    void reset_frame_counters();
    void reset_all();

private:
    struct resource_record {
        std::uint32_t generation = 0;
        std::uint64_t desc_hash = 0;
        std::uint64_t logical_hash = 0;
        bool alive = false;
    };

    struct view_record {
        std::uint64_t resource = 0;
        std::uint32_t resource_generation = 0;
        bool alive = false;
    };

    struct pipeline_record {
        std::uint32_t generation = 0;
        std::uint64_t pixel_shader_hash = 0;
        std::uint32_t receiver_id = 0;
        std::uint64_t consumer_family_hash = 0;
        bool confirmed = false;
        bool alive = false;
    };

    struct transaction_record {
        bool active = false;
        operator_kind op = operator_kind::specrgb;
    };

    struct command_record {
        std::uint64_t bound_pipeline = 0;
        std::uint32_t bound_pipeline_generation = 0;
        std::uint64_t draw_serial = 0;
        transaction_record transaction{};
    };

    static constexpr std::size_t op_index(operator_kind op) noexcept {
        return static_cast<std::size_t>(op);
    }

    runtime_snapshot snapshot_locked() const;

    mutable std::recursive_mutex mutex_;
    std::unordered_map<std::uint64_t, resource_record> resources_;
    std::unordered_map<std::uint64_t, view_record> views_;
    std::unordered_map<std::uint64_t, pipeline_record> pipelines_;
    std::unordered_map<std::uint64_t, command_record> commands_;
    std::array<operator_counters, operator_count> counters_{};
};

} // namespace dsrrl::runtime_v2
