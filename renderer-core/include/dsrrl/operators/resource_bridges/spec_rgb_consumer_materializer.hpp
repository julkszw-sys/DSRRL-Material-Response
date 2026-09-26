#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::resource_bridges {

enum class spec_rgb_consumer_result : std::uint8_t {
    applied = 0,
    fail_invalid_dxbc,
    fail_declaration_identity,
    fail_sample_identity,
    fail_rdef,
    fail_rebuild,
    fail_postcondition
};

spec_rgb_consumer_result materialize_spec_rgb_consumer(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::resource_bridges
