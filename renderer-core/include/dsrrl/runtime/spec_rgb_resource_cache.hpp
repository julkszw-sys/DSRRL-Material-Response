#pragma once

#include "dsrrl/operators/resource_bridges/spec_rgb_bridge.hpp"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace dsrrl::runtime {

using spec_rgb_resource_handle = std::uint64_t;

enum class spec_rgb_resource_result : std::uint8_t {
    registered = 0,
    already_registered,
    fail_open_invalid_identity,
    fail_open_invalid_resource,
    fail_open_ambiguous_identity,
    fail_open_conflicting_stock_binding,
    fail_open_conflicting_companion
};

struct spec_rgb_resource_record {
    std::u16string logical_name;
    spec_rgb_resource_handle stock_t1_srv = 0;
    spec_rgb_resource_handle ptde_t10_srv = 0;
};

struct spec_rgb_bind_request {
    operators::resource_bridges::spec_rgb_context route{};
    std::u16string logical_name;
    spec_rgb_resource_handle currently_bound_t1 = 0;
};

struct spec_rgb_bind_plan {
    operators::resource_bridges::spec_rgb_decision route{};
    bool activate = false;
    spec_rgb_resource_handle preserved_t1_srv = 0;
    spec_rgb_resource_handle ptde_t10_srv = 0;
};

class spec_rgb_resource_cache {
public:
    spec_rgb_resource_result register_exact(
        const std::u16string &logical_name,
        spec_rgb_resource_handle stock_t1_srv,
        spec_rgb_resource_handle ptde_t10_srv);

    spec_rgb_bind_plan plan_bind(
        const spec_rgb_bind_request &request) const;

    void erase_stock(spec_rgb_resource_handle stock_t1_srv);
    void clear() noexcept;
    std::size_t size() const noexcept;

private:
    struct entry {
        std::u16string logical_name;
        spec_rgb_resource_handle stock_t1_srv = 0;
        spec_rgb_resource_handle ptde_t10_srv = 0;
        bool ambiguous = false;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::u16string, entry> by_name_;
    std::unordered_map<spec_rgb_resource_handle, std::u16string> name_by_stock_;
};

} // namespace dsrrl::runtime
