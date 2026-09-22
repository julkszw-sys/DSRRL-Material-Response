#include "runtime_core_v2.hpp"

namespace dsrrl::runtime_v2 {

route_decision tracker::evaluate_route(route_contract contract, route_observation observation) const noexcept {
    const std::uint32_t missing = contract.required & ~observation.verified;
    return {missing == evidence_none, missing};
}

void tracker::index_logical_locked(
    std::uint64_t handle,
    std::uint32_t generation,
    std::uint64_t logical_hash) {
    if (logical_hash == 0)
        return;

    logical_index_[logical_hash].push_back(resource_ref{handle, generation});
}

std::uint32_t tracker::init_resource(std::uint64_t handle, std::uint64_t desc_hash, std::uint64_t logical_hash) {
    if (handle == 0) return 0;
    std::lock_guard lock(mutex_);
    auto &r = resources_[handle];
    const bool was_alive = r.alive;
    const std::uint64_t old_logical_hash = r.logical_hash;

    if (!r.alive) {
        ++r.generation;
        if (r.generation == 0) ++r.generation;
    }

    r.desc_hash = desc_hash;
    r.logical_hash = logical_hash;
    r.alive = true;

    if (logical_hash != 0 && (!was_alive || old_logical_hash != logical_hash))
        index_logical_locked(handle, r.generation, logical_hash);

    return r.generation;
}

bool tracker::annotate_resource(std::uint64_t handle, std::uint64_t desc_hash, std::uint64_t logical_hash) {
    if (handle == 0) return false;
    std::lock_guard lock(mutex_);
    const auto it = resources_.find(handle);
    if (it == resources_.end() || !it->second.alive)
        return false;

    const std::uint64_t old_logical_hash = it->second.logical_hash;
    it->second.desc_hash = desc_hash;
    it->second.logical_hash = logical_hash;

    if (logical_hash != 0 && old_logical_hash != logical_hash)
        index_logical_locked(handle, it->second.generation, logical_hash);

    return true;
}

bool tracker::annotate_resource_logical(std::uint64_t handle, std::uint64_t logical_hash) {
    if (handle == 0 || logical_hash == 0) return false;
    std::lock_guard lock(mutex_);
    const auto it = resources_.find(handle);
    if (it == resources_.end() || !it->second.alive)
        return false;

    if (it->second.logical_hash != logical_hash) {
        it->second.logical_hash = logical_hash;
        index_logical_locked(handle, it->second.generation, logical_hash);
    }

    return true;
}

void tracker::destroy_resource(std::uint64_t handle) {
    if (handle == 0) return;
    std::lock_guard lock(mutex_);
    if (auto it = resources_.find(handle); it != resources_.end())
        it->second.alive = false;
}

bool tracker::init_view(std::uint64_t view, std::uint64_t resource) {
    if (view == 0 || resource == 0) return false;
    std::lock_guard lock(mutex_);
    const auto rit = resources_.find(resource);
    if (rit == resources_.end() || !rit->second.alive)
        return false;

    auto &v = views_[view];
    if (!v.alive) {
        ++v.generation;
        if (v.generation == 0) ++v.generation;
    }

    v.resource = resource;
    v.resource_generation = rit->second.generation;
    v.alive = true;
    return true;
}

void tracker::destroy_view(std::uint64_t view) {
    if (view == 0) return;
    std::lock_guard lock(mutex_);
    if (auto it = views_.find(view); it != views_.end())
        it->second.alive = false;
}

std::optional<resource_identity> tracker::resolve_view(std::uint64_t view, operator_kind op) {
    std::lock_guard lock(mutex_);
    const auto vit = views_.find(view);
    if (vit == views_.end() || !vit->second.alive)
        return std::nullopt;

    const auto rit = resources_.find(vit->second.resource);
    if (rit == resources_.end() || !rit->second.alive ||
        rit->second.generation != vit->second.resource_generation) {
        ++counters_[op_index(op)].stale_view;
        return std::nullopt;
    }

    return resource_identity{
        vit->second.resource,
        rit->second.generation,
        rit->second.desc_hash,
        rit->second.logical_hash
    };
}

std::optional<resource_identity> tracker::resolve_logical_resource(
    std::uint64_t logical_hash,
    operator_kind op) {
    std::lock_guard lock(mutex_);
    auto &counter = counters_[op_index(op)];

    if (logical_hash == 0) {
        ++counter.logical_miss;
        return std::nullopt;
    }

    const auto iit = logical_index_.find(logical_hash);
    if (iit == logical_index_.end()) {
        ++counter.logical_miss;
        return std::nullopt;
    }

    std::optional<resource_identity> result;

    for (const resource_ref ref : iit->second) {
        const auto rit = resources_.find(ref.handle);
        if (rit == resources_.end() ||
            !rit->second.alive ||
            rit->second.generation != ref.generation ||
            rit->second.logical_hash != logical_hash)
            continue;

        if (result.has_value() &&
            (result->handle != ref.handle || result->generation != ref.generation)) {
            ++counter.logical_ambiguous;
            return std::nullopt;
        }

        result = resource_identity{
            ref.handle,
            rit->second.generation,
            rit->second.desc_hash,
            rit->second.logical_hash
        };
    }

    if (!result.has_value())
        ++counter.logical_miss;

    return result;
}

std::uint32_t tracker::init_pipeline(std::uint64_t handle, std::uint64_t pixel_shader_hash,
                                     std::uint32_t receiver_id, std::uint64_t consumer_family_hash,
                                     bool confirmed) {
    if (handle == 0) return 0;
    std::lock_guard lock(mutex_);
    auto &p = pipelines_[handle];
    if (!p.alive) {
        ++p.generation;
        if (p.generation == 0) ++p.generation;
    }
    p.pixel_shader_hash = pixel_shader_hash;
    p.receiver_id = receiver_id;
    p.consumer_family_hash = consumer_family_hash;
    p.confirmed = confirmed;
    p.alive = true;
    return p.generation;
}

void tracker::destroy_pipeline(std::uint64_t handle) {
    if (handle == 0) return;
    std::lock_guard lock(mutex_);
    if (auto it = pipelines_.find(handle); it != pipelines_.end())
        it->second.alive = false;
}

std::optional<pipeline_identity> tracker::resolve_pipeline(std::uint64_t handle) const {
    std::lock_guard lock(mutex_);
    const auto it = pipelines_.find(handle);
    if (it == pipelines_.end() || !it->second.alive)
        return std::nullopt;

    const auto &p = it->second;
    return pipeline_identity{
        handle,
        p.generation,
        p.pixel_shader_hash,
        p.receiver_id,
        p.consumer_family_hash,
        p.confirmed
    };
}

void tracker::init_command(std::uint64_t command) {
    if (command == 0) return;
    std::lock_guard lock(mutex_);
    commands_.try_emplace(command);
}

void tracker::destroy_command(std::uint64_t command) {
    if (command == 0) return;
    std::lock_guard lock(mutex_);
    const auto it = commands_.find(command);
    if (it == commands_.end()) return;

    if (it->second.transaction.active) {
        auto &c = counters_[op_index(it->second.transaction.op)];
        ++c.unrestored;
    }

    commands_.erase(it);
}

bool tracker::bind_pipeline(std::uint64_t command, std::uint64_t pipeline) {
    if (command == 0) return false;
    std::lock_guard lock(mutex_);
    auto &cmd = commands_[command];
    const auto pit = pipelines_.find(pipeline);

    if (pit == pipelines_.end() || !pit->second.alive) {
        cmd.bound_pipeline = 0;
        cmd.bound_pipeline_generation = 0;
        return false;
    }

    cmd.bound_pipeline = pipeline;
    cmd.bound_pipeline_generation = pit->second.generation;
    return true;
}

std::uint64_t tracker::begin_draw(std::uint64_t command) {
    if (command == 0) return 0;
    std::lock_guard lock(mutex_);
    auto &cmd = commands_[command];
    return ++cmd.draw_serial;
}

std::optional<command_snapshot> tracker::command_state(std::uint64_t command) const {
    std::lock_guard lock(mutex_);
    const auto it = commands_.find(command);
    if (it == commands_.end()) return std::nullopt;

    const auto &c = it->second;
    return command_snapshot{
        command,
        c.bound_pipeline,
        c.bound_pipeline_generation,
        c.draw_serial,
        c.transaction.active,
        c.transaction.op
    };
}

std::optional<pipeline_identity> tracker::resolve_bound_pipeline(std::uint64_t command) const {
    std::lock_guard lock(mutex_);
    const auto cit = commands_.find(command);
    if (cit == commands_.end() || cit->second.bound_pipeline == 0)
        return std::nullopt;

    const auto pit = pipelines_.find(cit->second.bound_pipeline);
    if (pit == pipelines_.end() ||
        !pit->second.alive ||
        pit->second.generation != cit->second.bound_pipeline_generation)
        return std::nullopt;

    const auto &p = pit->second;
    return pipeline_identity{
        cit->second.bound_pipeline,
        p.generation,
        p.pixel_shader_hash,
        p.receiver_id,
        p.consumer_family_hash,
        p.confirmed
    };
}

bool tracker::bind_pixel_shader_views(
    std::uint64_t command,
    std::uint32_t first,
    std::uint32_t count,
    const std::uint64_t *views) {
    if (command == 0 ||
        (count != 0 && views == nullptr) ||
        first > max_pixel_shader_resource_slots ||
        count > max_pixel_shader_resource_slots - first)
        return false;

    std::lock_guard lock(mutex_);
    const auto cit = commands_.find(command);
    if (cit == commands_.end())
        return false;

    bool complete = true;

    for (std::uint32_t i = 0; i < count; ++i) {
        auto &dst = cit->second.pixel_shader_views[first + i];
        const std::uint64_t view = views[i];

        if (view == 0) {
            dst = {};
            continue;
        }

        const auto vit = views_.find(view);
        if (vit == views_.end() || !vit->second.alive) {
            dst = {};
            complete = false;
            continue;
        }

        dst = bound_view_ref{view, vit->second.generation};
    }

    return complete;
}

std::optional<resource_identity> tracker::resolve_bound_pixel_shader_resource(
    std::uint64_t command,
    std::uint32_t slot,
    operator_kind op) {
    if (slot >= max_pixel_shader_resource_slots)
        return std::nullopt;

    std::lock_guard lock(mutex_);
    const auto cit = commands_.find(command);
    if (cit == commands_.end())
        return std::nullopt;

    const bound_view_ref ref = cit->second.pixel_shader_views[slot];
    if (ref.handle == 0)
        return std::nullopt;

    const auto vit = views_.find(ref.handle);
    if (vit == views_.end() ||
        !vit->second.alive ||
        vit->second.generation != ref.generation) {
        ++counters_[op_index(op)].stale_view;
        return std::nullopt;
    }

    const auto rit = resources_.find(vit->second.resource);
    if (rit == resources_.end() ||
        !rit->second.alive ||
        rit->second.generation != vit->second.resource_generation) {
        ++counters_[op_index(op)].stale_view;
        return std::nullopt;
    }

    return resource_identity{
        vit->second.resource,
        rit->second.generation,
        rit->second.desc_hash,
        rit->second.logical_hash
    };
}

void tracker::note_receiver_match(operator_kind op) {
    std::lock_guard lock(mutex_);
    ++counters_[op_index(op)].receiver_matches;
}

void tracker::note_resource_match(operator_kind op) {
    std::lock_guard lock(mutex_);
    ++counters_[op_index(op)].resource_matches;
}

void tracker::note_stage(operator_kind op, route_stage stage) {
    const std::size_t stage_index = static_cast<std::size_t>(stage);
    if (stage_index >= route_stage_count)
        return;

    std::lock_guard lock(mutex_);
    ++stages_[op_index(op)].hits[stage_index];
}

bool tracker::begin_transaction(std::uint64_t command, operator_kind op,
                                route_contract contract, route_observation observation) {
    std::lock_guard lock(mutex_);
    auto &counter = counters_[op_index(op)];
    const route_decision decision = evaluate_route(contract, observation);

    if (!decision.allowed) {
        ++counter.rejected;
        ++counter.fail_open;
        return false;
    }

    auto it = commands_.find(command);
    if (it == commands_.end() || it->second.transaction.active) {
        ++counter.rejected;
        ++counter.fail_open;
        return false;
    }

    it->second.transaction = transaction_record{true, op};
    ++counter.activations;
    return true;
}

bool tracker::restore_transaction(std::uint64_t command, operator_kind op) {
    std::lock_guard lock(mutex_);
    const auto it = commands_.find(command);

    if (it == commands_.end() ||
        !it->second.transaction.active ||
        it->second.transaction.op != op) {
        ++counters_[op_index(op)].restore_faults;
        return false;
    }

    it->second.transaction.active = false;
    ++counters_[op_index(op)].restores;
    return true;
}

void tracker::note_fail_open(operator_kind op) {
    std::lock_guard lock(mutex_);
    ++counters_[op_index(op)].fail_open;
}

runtime_snapshot tracker::snapshot_locked() const {
    runtime_snapshot out{};
    out.operators = counters_;
    out.stages = stages_;

    for (const auto &[_, r] : resources_) {
        if (!r.alive)
            continue;

        ++out.live_resources;
        if (r.logical_hash != 0)
            ++out.live_logical_resources;
    }
    for (const auto &[_, v] : views_)
        if (v.alive) ++out.live_views;
    for (const auto &[_, p] : pipelines_)
        if (p.alive) ++out.live_pipelines;

    out.live_commands = commands_.size();
    return out;
}

runtime_snapshot tracker::seal_frame() {
    std::lock_guard lock(mutex_);

    for (auto &[_, cmd] : commands_) {
        if (!cmd.transaction.active)
            continue;

        auto &counter = counters_[op_index(cmd.transaction.op)];
        ++counter.unrestored;
        cmd.transaction.active = false;
    }

    return snapshot_locked();
}

runtime_snapshot tracker::snapshot() const {
    std::lock_guard lock(mutex_);
    return snapshot_locked();
}

void tracker::reset_frame_counters() {
    std::lock_guard lock(mutex_);
    counters_ = {};
    stages_ = {};
}

void tracker::reset_all() {
    std::lock_guard lock(mutex_);
    resources_.clear();
    logical_index_.clear();
    views_.clear();
    pipelines_.clear();
    commands_.clear();
    counters_ = {};
    stages_ = {};
}

} // namespace dsrrl::runtime_v2
