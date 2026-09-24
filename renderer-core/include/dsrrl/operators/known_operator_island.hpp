#pragma once

#include "dsrrl/core/island_policy.hpp"

namespace dsrrl::operators {

class known_operator_island {
public:
    explicit constexpr known_operator_island(core::operator_id id) noexcept : id_(id) {}

    core::operator_id id() const noexcept { return id_; }

    core::activation_decision evaluate(
        const core::feature_registry &features,
        const core::activation_context &context) const noexcept
    {
        return core::evaluate_operator_activation(features, id_, context);
    }

private:
    core::operator_id id_;
};

} // namespace dsrrl::operators
