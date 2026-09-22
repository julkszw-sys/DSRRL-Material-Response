#include "runtime_core_v2.hpp"

namespace dsrrl::runtime_v2 {

route_decision tracker::evaluate_route(route_contract contract, route_observation observation) const noexcept {
    const std::uint32_t missing = contract.required & ~observation.verified;
    return {missing == evidence_none, missing};
}

std::uint32_t tracker::init_resource(std::uint64_t handle, std::uint64_t desc_hash, std::uint64_t logical_hash) {
    if (handle == 0) return 0;
    std::lock_guard lock(mutex_);
    auto &r = resources_[handle];
    if (!r.alive) {
        ++r.generation;
        if (r.generation == 0) ++r.generation;
    }
    r.desc_hash = desc_hash;
    r.logical_hash = logical_hash;
    r.alive = true;
    return r.generation;
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
    views_[view] = view_record{resource, rit->second.generation, true};
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
        ++c.fail_open;
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

void tracker::note_receiver_match(operator_kind op) {
    std::lock_guard lock(mutex_);
    ++counters_[op_index(op)].receiver_matches;
}

void tracker::note_resource_match(operator_kind op) {
    std::lock_guard lock(mutex_);
    ++counters_[op_index(op)].resource_matches;
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
        ++counters_[op_index(op)].fail_open;
        return false;
    }

    it->second.transaction.active = false;
    ++counters_[op_index(op)].restores;
    return true;
}

void tracker::fail_open(std::uint64_t command, operator_kind op) {
    std::lock_guard lock(mutex_);
    ++counters_[op_index(op)].fail_open;

    if (auto it = commands_.find(command);
        it != commands_.end() &&
        it->second.transaction.active &&
        it->second.transaction.op == op) {
        it->second.transaction.active = false;
    }
}

runtime_snapshot tracker::snapshot_locked() const {
    runtime_snapshot out{};
    out.operators = counters_;

    for (const auto &[_, r] : resources_)
        if (r.alive) ++out.live_resources;
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
        ++counter.fail_open;
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
}

} // namespace dsrrl::runtime_v2
